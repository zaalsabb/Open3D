// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2020 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "open3d/utility/RemoteFunctions.h"
#include "open3d/utility/Console.h"

namespace open3d {
namespace utility {

messages::Status UnpackStatusFromReply(const zmq::message_t& msg,
                                       size_t& offset,
                                       bool& ok) {
    ok = false;
    if (msg.size() <= offset) {
        return messages::Status();
    };

    messages::Reply reply;
    messages::Status status;
    try {
        auto obj_handle =
                msgpack::unpack((char*)msg.data(), msg.size(), offset);
        obj_handle.get().convert(reply);
        if (reply.msg_id != status.MsgId()) {
            LogDebug("Expected msg with id {} but got {}", status.MsgId(),
                     reply.msg_id);
        } else {
            auto status_obj_handle =
                    msgpack::unpack((char*)msg.data(), msg.size(), offset);
            status_obj_handle.get().convert(status);
            ok = true;
        }
    } catch (std::exception& e) {
        LogDebug("Failed to parse message: {}", e.what());
        offset = msg.size();
    }
    return status;
}

bool ReplyIsOKStatus(const zmq::message_t& msg) {
    size_t offset = 0;
    return ReplyIsOKStatus(msg, offset);
}

bool ReplyIsOKStatus(const zmq::message_t& msg, size_t& offset) {
    bool ok;
    auto status = UnpackStatusFromReply(msg, offset, ok);
    if (ok && 0 == status.code) {
        return true;
    }
    return false;
}

bool SetPointCloud(const open3d::geometry::PointCloud& pcd,
                   const std::string& path,
                   int time,
                   const std::string& layer,
                   std::shared_ptr<Connection> connection) {
    // TODO use SetMeshData here after switching to the new PointCloud class.
    if (pcd.HasPoints() == 0) {
        LogInfo("SetMeshData: point cloud is empty");
        return false;
    }

    messages::SetMeshData msg;
    msg.path = path;
    msg.time = time;
    msg.layer = layer;

    msg.data.vertices = messages::Array::FromPtr(
            (double*)pcd.points_.data(), {int64_t(pcd.points_.size()), 3});
    if (pcd.HasNormals()) {
        msg.data.vertex_attributes["normals"] =
                messages::Array::FromPtr((double*)pcd.normals_.data(),
                                         {int64_t(pcd.normals_.size()), 3});
    }
    if (pcd.HasColors()) {
        msg.data.vertex_attributes["colors"] = messages::Array::FromPtr(
                (double*)pcd.colors_.data(), {int64_t(pcd.colors_.size()), 3});
    }

    msgpack::sbuffer sbuf;
    messages::Request request{msg.MsgId()};
    msgpack::pack(sbuf, request);
    msgpack::pack(sbuf, msg);

    if (!connection) {
        connection = std::make_shared<Connection>();
    }
    zmq::message_t send_msg(sbuf.data(), sbuf.size());
    auto reply = connection->Send(send_msg);
    return ReplyIsOKStatus(*reply);
}

bool SetMeshData(
        const open3d::core::Tensor& vertices,
        const std::string& path,
        int time,
        const std::string& layer,
        const std::map<std::string, open3d::core::Tensor>& vertex_attributes,
        const open3d::core::Tensor& faces,
        const std::map<std::string, open3d::core::Tensor>& face_attributes,
        const open3d::core::Tensor& lines,
        const std::map<std::string, open3d::core::Tensor>& line_attributes,
        const std::map<std::string, open3d::core::Tensor>& textures,
        std::shared_ptr<Connection> connection) {
    using namespace open3d::core;

    if (vertices.NumElements() == 0) {
        LogInfo("SetMeshData: vertices Tensor is empty");
        return false;
    }
    if (vertices.NumDims() != 2) {
        LogInfo("SetMeshData: vertices ndim must be 2 but is {}",
                vertices.NumDims());
        return false;
    }
    if (vertices.GetDtype() != Dtype::Float32 &&
        vertices.GetDtype() != Dtype::Float64) {
        LogError(
                "SetMeshData: vertices must have dtype Float32 or Float64 but "
                "is {}",
                DtypeUtil::ToString(vertices.GetDtype()));
    }

    auto PrepareTensor = [](const Tensor& a) {
        if (a.GetDevice().GetType() != Device::DeviceType::CPU) {
            Device cpu_device;
            return a.Copy(cpu_device);
        } else if (!a.IsContiguous()) {
            return a.Contiguous();
        }
        return a;
    };

    auto CreateArray = [](const Tensor& a) {
        switch (a.GetDtype()) {
            case Dtype::Float32:
                return messages::Array::FromPtr(
                        (float*)a.GetDataPtr(),
                        static_cast<std::vector<int64_t>>(a.GetShape()));
            case Dtype::Float64:
                return messages::Array::FromPtr(
                        (double*)a.GetDataPtr(),
                        static_cast<std::vector<int64_t>>(a.GetShape()));
            case Dtype::Int32:
                return messages::Array::FromPtr(
                        (int32_t*)a.GetDataPtr(),
                        static_cast<std::vector<int64_t>>(a.GetShape()));
            case Dtype::Int64:
                return messages::Array::FromPtr(
                        (int64_t*)a.GetDataPtr(),
                        static_cast<std::vector<int64_t>>(a.GetShape()));
            case Dtype::UInt8:
                return messages::Array::FromPtr(
                        (uint8_t*)a.GetDataPtr(),
                        static_cast<std::vector<int64_t>>(a.GetShape()));
            default:
                LogError("Unsupported dtype {}",
                         DtypeUtil::ToString(a.GetDtype()));
                return messages::Array();
        };

    };

    messages::SetMeshData msg;
    msg.path = path;
    msg.time = time;
    msg.layer = layer;

    const Tensor vertices_ok = PrepareTensor(vertices);
    msg.data.vertices = CreateArray(vertices_ok);

    for (const auto& item : vertex_attributes) {
        const Tensor tensor = PrepareTensor(item.second);
        if (tensor.NumDims() >= 1 &&
            tensor.GetShape()[0] == vertices.GetShape()[0]) {
            msg.data.vertex_attributes[item.first] = CreateArray(item.second);
        } else {
            LogError("SetMeshData: Attribute {} has incompatible shape {}",
                     item.first, tensor.GetShape().ToString());
        }
    }

    if (faces.NumElements()) {
        if (faces.GetDtype() != Dtype::Int32 &&
            faces.GetDtype() != Dtype::Int64) {
            LogError(
                    "SetMeshData: faces must have dtype Int32 or Int64 but "
                    "is {}",
                    DtypeUtil::ToString(vertices.GetDtype()));
        } else if (faces.NumDims() != 2) {
            LogError("SetMeshData: faces must have rank 2 but is {}",
                     faces.NumDims());
        } else if (faces.GetShape()[1] >= 3) {
            LogError("SetMeshData: last dim of faces must be >3 but is {}",
                     faces.GetShape()[1]);
        } else {
            const Tensor faces_ok = PrepareTensor(faces);
            msg.data.faces = CreateArray(faces_ok);

            for (const auto& item : face_attributes) {
                const Tensor tensor = PrepareTensor(item.second);
                if (tensor.NumDims() >= 1 &&
                    tensor.GetShape()[0] == faces.GetShape()[0]) {
                    msg.data.face_attributes[item.first] =
                            CreateArray(item.second);
                } else {
                    LogError(
                            "SetMeshData: Attribute {} has incompatible shape "
                            "{}",
                            item.first, tensor.GetShape().ToString());
                }
            }
        }
    }

    if (lines.NumElements()) {
        if (lines.GetDtype() != Dtype::Int32 &&
            lines.GetDtype() != Dtype::Int64) {
            LogError(
                    "SetMeshData: lines must have dtype Int32 or Int64 but "
                    "is {}",
                    DtypeUtil::ToString(vertices.GetDtype()));
        } else if (lines.NumDims() != 2) {
            LogError("SetMeshData: lines must have rank 2 but is {}",
                     lines.NumDims());
        } else if (lines.GetShape()[1] >= 2) {
            LogError("SetMeshData: last dim of lines must be >2 but is {}",
                     lines.GetShape()[1]);
        } else {
            const Tensor lines_ok = PrepareTensor(lines);
            msg.data.lines = CreateArray(lines_ok);

            for (const auto& item : line_attributes) {
                const Tensor tensor = PrepareTensor(item.second);
                if (tensor.NumDims() >= 1 &&
                    tensor.GetShape()[0] == lines.GetShape()[0]) {
                    msg.data.line_attributes[item.first] =
                            CreateArray(item.second);
                } else {
                    LogError(
                            "SetMeshData: Attribute {} has incompatible shape "
                            "{}",
                            item.first, tensor.GetShape().ToString());
                }
            }
        }
    }

    for (const auto& item : textures) {
        const Tensor tensor = PrepareTensor(item.second);
        if (tensor.NumElements()) {
            msg.data.textures[item.first] = CreateArray(item.second);
        } else {
            LogError("SetMeshData: Texture {} is empty", item.first);
        }
    }

    msgpack::sbuffer sbuf;
    messages::Request request{msg.MsgId()};
    msgpack::pack(sbuf, request);
    msgpack::pack(sbuf, msg);

    if (!connection) {
        connection = std::make_shared<Connection>();
    }
    zmq::message_t send_msg(sbuf.data(), sbuf.size());
    auto reply = connection->Send(send_msg);
    return ReplyIsOKStatus(*reply);
}

}  // namespace utility
}  // namespace open3d