/*
 * Copyright 2019 The GraphicsFuzz Project Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "amber_scoop/layer.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "amber_scoop/vk_deep_copy.h"
#include "common/spirv_util.h"
#include "spirv-tools/libspirv.hpp"

namespace graphicsfuzz_amber_scoop {

#define DEBUG_AMBER_SCOOP 0

#if DEBUG_AMBER_SCOOP
#define DEBUG_LAYER(F) std::cout << "In " << #F << std::endl
#else
#define DEBUG_LAYER(F)
#endif

struct CmdBeginRenderPass;
struct CmdBindDescriptorSets;
struct CmdBindIndexBuffer;
struct CmdBindPipeline;
struct CmdBindVertexBuffers;
struct CmdCopyBuffer;
struct CmdDraw;
struct CmdDrawIndexed;
struct BufferCopy;

enum FormatType {
  tInt8 = 0,
  tInt16 = 1,
  tInt32 = 2,
  tInt64 = 3,
  tUint8 = 4,
  tUint16 = 5,
  tUint32 = 6,
  tUint64 = 7,
  tFloat = 8,
  tDouble = 9
};

const std::map<VkPrimitiveTopology, std::string> topologies = {
    {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, "POINT_LIST"},
    {VK_PRIMITIVE_TOPOLOGY_LINE_LIST, "LINE_LIST"},
    {VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, "LINE_STRIP"},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "TRIANGLE_LIST"},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, "TRIANGLE_STRIP"},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, "TRIANGLE_FAN"},
    {VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
     "LINE_LIST_WITH_ADJACENCY"},
    {VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
     "LINE_STRIP_WITH_ADJACENCY"},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
     "TRIANGLE_LIST_WITH_ADJACENCY"},
    {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
     "TRIANGLE_STRIP_WITH_ADJACENCY"},
    {VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, "PATCH_LIST"}};

struct Cmd {

  enum Kind {
    kBeginRenderPass,
    kBindDescriptorSets,
    kBindIndexBuffer,
    kBindPipeline,
    kBindVertexBuffers,
    kCopyBuffer,
    kDraw,
    kDrawIndexed
  };

  explicit Cmd(Kind kind) : kind_(kind) {}

// A bunch of methods for casting this type to a given type. Returns this if the
// cast can be done, nullptr otherwise.
// clang-format off
#define DeclareCastMethod(target)                                              \
  virtual Cmd##target *As##target() { return nullptr; }                        \
  virtual const Cmd##target *As##target() const { return nullptr; }
  DeclareCastMethod(BeginRenderPass)
  DeclareCastMethod(BindDescriptorSets)
  DeclareCastMethod(BindIndexBuffer)
  DeclareCastMethod(BindPipeline)
  DeclareCastMethod(BindVertexBuffers)
  DeclareCastMethod(CopyBuffer)
  DeclareCastMethod(Draw)
  DeclareCastMethod(DrawIndexed)
#undef DeclareCastMethod

  Kind kind_;
  // clang-format on
};

struct CmdBeginRenderPass : public Cmd {
  CmdBeginRenderPass(VkRenderPassBeginInfo const *pRenderPassBegin,
                     VkSubpassContents contents)
      : Cmd(kBeginRenderPass), pRenderPassBegin_(DeepCopy(pRenderPassBegin)),
        contents_(contents) {}

  CmdBeginRenderPass *AsBeginRenderPass() override { return this; }
  const CmdBeginRenderPass *AsBeginRenderPass() const override { return this; }

  VkRenderPassBeginInfo *pRenderPassBegin_;
  VkSubpassContents contents_;
};

struct CmdBindDescriptorSets : public Cmd {

  CmdBindDescriptorSets(VkPipelineBindPoint pipelineBindPoint,
                        VkPipelineLayout layout, uint32_t firstSet,
                        uint32_t descriptorSetCount,
                        VkDescriptorSet const *pDescriptorSets,
                        uint32_t dynamicOffsetCount,
                        uint32_t const *pDynamicOffsets)
      : Cmd(kBindDescriptorSets), pipelineBindPoint_(pipelineBindPoint),
        layout_(layout), firstSet_(firstSet),
        descriptorSetCount_(descriptorSetCount),
        pDescriptorSets_(CopyArray(pDescriptorSets, descriptorSetCount)),
        dynamicOffsetCount_(dynamicOffsetCount),
        pDynamicOffsets_(CopyArray(pDynamicOffsets, dynamicOffsetCount)) {}

  CmdBindDescriptorSets *AsBindDescriptorSets() override { return this; }
  const CmdBindDescriptorSets *AsBindDescriptorSets() const override {
    return this;
  }

  VkPipelineBindPoint pipelineBindPoint_;
  VkPipelineLayout layout_;
  uint32_t firstSet_;
  uint32_t descriptorSetCount_;
  VkDescriptorSet *pDescriptorSets_;
  uint32_t dynamicOffsetCount_;
  uint32_t *pDynamicOffsets_;
};

struct CmdBindIndexBuffer : public Cmd {

  CmdBindIndexBuffer(VkBuffer buffer, VkDeviceSize offset,
                     VkIndexType indexType)
      : Cmd(kBindIndexBuffer), buffer_(buffer), offset_(offset),
        indexType_(indexType) {}

  CmdBindIndexBuffer *AsBindIndexBuffer() override { return this; }
  const CmdBindIndexBuffer *AsBindIndexBuffer() const override { return this; }

  VkBuffer buffer_;
  VkDeviceSize offset_;
  VkIndexType indexType_;
};

struct CmdBindPipeline : public Cmd {
  CmdBindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
      : Cmd(kBindPipeline), pipelineBindPoint_(pipelineBindPoint),
        pipeline_(pipeline) {}

  CmdBindPipeline *AsBindPipeline() override { return this; }
  const CmdBindPipeline *AsBindPipeline() const override { return this; }

  VkPipelineBindPoint pipelineBindPoint_;
  VkPipeline pipeline_;
};

struct CmdBindVertexBuffers : public Cmd {

  CmdBindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount,
                       VkBuffer const *pBuffers, VkDeviceSize const *pOffsets)
      : Cmd(kBindVertexBuffers), firstBinding_(firstBinding),
        bindingCount_(bindingCount),
        pBuffers_(CopyArray(pBuffers, bindingCount)),
        pOffsets_(CopyArray(pOffsets, bindingCount)) {}

  CmdBindVertexBuffers *AsBindVertexBuffers() override { return this; }
  const CmdBindVertexBuffers *AsBindVertexBuffers() const override {
    return this;
  }

  uint32_t firstBinding_;
  uint32_t bindingCount_;
  VkBuffer const *pBuffers_;
  VkDeviceSize const *pOffsets_;
};

struct CmdCopyBuffer : public Cmd {
  CmdCopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                VkBufferCopy const *pRegions)
      : Cmd(kCopyBuffer), srcBuffer_(srcBuffer), dstBuffer_(dstBuffer),
        regionCount_(regionCount), pRegions_(pRegions) {}

  CmdCopyBuffer *AsCopyBuffer() override { return this; }
  const CmdCopyBuffer *AsCopyBuffer() const override { return this; }

  VkBuffer srcBuffer_;
  VkBuffer dstBuffer_;
  uint32_t regionCount_;
  VkBufferCopy const *pRegions_;
};

struct CmdDraw : public Cmd {
  CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
          uint32_t firstInstance)
      : Cmd(kDraw), vertexCount_(vertexCount), instanceCount_(instanceCount),
        firstVertex_(firstVertex), firstInstance_(firstInstance) {}

  CmdDraw *AsDraw() override { return this; }
  const CmdDraw *AsDraw() const override { return this; }

  uint32_t vertexCount_;
  uint32_t instanceCount_;
  uint32_t firstVertex_;
  uint32_t firstInstance_;
};

struct CmdDrawIndexed : public Cmd {
  CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                 uint32_t firstIndex, int32_t vertexOffset,
                 uint32_t firstInstance)
      : Cmd(kDrawIndexed), indexCount_(indexCount),
        instanceCount_(instanceCount), firstIndex_(firstIndex),
        vertexOffset_(vertexOffset), firstInstance_(firstInstance) {}

  CmdDrawIndexed *AsDrawIndexed() override { return this; }
  const CmdDrawIndexed *AsDrawIndexed() const override { return this; }

  uint32_t indexCount_;
  uint32_t instanceCount_;
  uint32_t firstIndex_;
  int32_t vertexOffset_;
  uint32_t firstInstance_;
};

std::unordered_map<VkCommandBuffer, std::vector<std::unique_ptr<Cmd>>>
    command_buffers;

void AddCommand(VkCommandBuffer command_buffer, std::unique_ptr<Cmd> command) {
  if (command_buffers.count(command_buffer) == 0) {
    std::vector<std::unique_ptr<Cmd>> empty_cmds;
    command_buffers.insert({command_buffer, std::move(empty_cmds)});
  }
  command_buffers.at(command_buffer).push_back(std::move(command));
}

std::unordered_map<VkDeviceMemory, std::tuple<VkDeviceSize, VkDeviceSize,
                                              VkMemoryMapFlags, void *>>
    mappedMemory;
std::unordered_map<VkBuffer, std::pair<VkDeviceMemory, VkDeviceSize>>
    bufferToMemory;
std::unordered_map<VkBuffer, VkBufferCreateInfo> buffers;
std::unordered_map<VkDescriptorSet, VkDescriptorSetLayout> descriptorSets;
std::unordered_map<VkDescriptorSetLayout, VkDescriptorSetLayoutCreateInfo>
    descriptorSetLayouts;
std::unordered_map<VkFramebuffer, VkFramebufferCreateInfo> framebuffers;
std::unordered_map<VkPipeline, VkGraphicsPipelineCreateInfo> graphicsPipelines;
std::unordered_map<VkPipelineLayout, VkPipelineLayoutCreateInfo>
    pipelineLayouts;
std::unordered_map<VkRenderPass, VkRenderPassCreateInfo> renderPasses;
std::unordered_map<VkShaderModule, VkShaderModuleCreateInfo> shaderModules;
std::unordered_map<VkDescriptorSet,
                   std::unordered_map<uint32_t, VkDescriptorBufferInfo>>
    descriptorSetToBindingBuffer;

std::vector<std::shared_ptr<BufferCopy>> bufferCopies;

std::string GetDisassembly(VkShaderModule shaderModule) {
  auto createInfo = shaderModules.at(shaderModule);
  auto maybeTargetEnv = graphicsfuzz_vulkan_layers::GetTargetEnvFromSpirvBinary(
      createInfo.pCode[1]);
  assert(maybeTargetEnv.first && "SPIR-V version should be valid.");
  spvtools::SpirvTools tools(maybeTargetEnv.second);
  assert(tools.IsValid() && "Invalid tools object created.");
  // |createInfo.codeSize| gives the size in bytes; convert it to words.
  const uint32_t code_size_in_words =
      static_cast<uint32_t>(createInfo.codeSize) / 4;
  std::vector<uint32_t> binary;
  binary.assign(createInfo.pCode, createInfo.pCode + code_size_in_words);
  std::string disassembly;
  tools.Disassemble(binary, &disassembly, SPV_BINARY_TO_TEXT_OPTION_INDENT);
  return disassembly;
}

VkResult
vkAllocateDescriptorSets(PFN_vkAllocateDescriptorSets next, VkDevice device,
                         VkDescriptorSetAllocateInfo const *pAllocateInfo,
                         VkDescriptorSet *pDescriptorSets) {
  DEBUG_LAYER(vkAllocateDescriptorSets);
  auto result = next(device, pAllocateInfo, pDescriptorSets);
  if (result == VK_SUCCESS) {
    for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++) {
      descriptorSets.insert(
          {pDescriptorSets[i], pAllocateInfo->pSetLayouts[i]});
    }
  }
  return result;
}

VkResult vkBindBufferMemory(PFN_vkBindBufferMemory next, VkDevice device,
                            VkBuffer buffer, VkDeviceMemory memory,
                            VkDeviceSize memoryOffset) {
  DEBUG_LAYER(vkBindBufferMemory);
  auto result = next(device, buffer, memory, memoryOffset);
  if (result == VK_SUCCESS) {
    bufferToMemory.insert({buffer, {memory, memoryOffset}});
  }
  return result;
}

void vkCmdBeginRenderPass(PFN_vkCmdBeginRenderPass next,
                          VkCommandBuffer commandBuffer,
                          VkRenderPassBeginInfo const *pRenderPassBegin,
                          VkSubpassContents contents) {
  DEBUG_LAYER(vkCmdBeginRenderPass);
  next(commandBuffer, pRenderPassBegin, contents);
  AddCommand(commandBuffer,
             std::make_unique<CmdBeginRenderPass>(pRenderPassBegin, contents));
}

void vkCmdBindDescriptorSets(PFN_vkCmdBindDescriptorSets next,
                             VkCommandBuffer commandBuffer,
                             VkPipelineBindPoint pipelineBindPoint,
                             VkPipelineLayout layout, uint32_t firstSet,
                             uint32_t descriptorSetCount,
                             VkDescriptorSet const *pDescriptorSets,
                             uint32_t dynamicOffsetCount,
                             uint32_t const *pDynamicOffsets) {
  DEBUG_LAYER(vkCmdBindDescriptorSets);
  next(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount,
       pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
  AddCommand(commandBuffer,
             std::make_unique<CmdBindDescriptorSets>(
                 pipelineBindPoint, layout, firstSet, descriptorSetCount,
                 pDescriptorSets, dynamicOffsetCount, pDynamicOffsets));
}

void vkCmdBindIndexBuffer(PFN_vkCmdBindIndexBuffer next,
                          VkCommandBuffer commandBuffer, VkBuffer buffer,
                          VkDeviceSize offset, VkIndexType indexType) {
  DEBUG_LAYER(vkCmdBindDescriptorSets);
  next(commandBuffer, buffer, offset, indexType);
  AddCommand(commandBuffer,
             std::make_unique<CmdBindIndexBuffer>(buffer, offset, indexType));
}
void vkCmdBindPipeline(PFN_vkCmdBindPipeline next,
                       VkCommandBuffer commandBuffer,
                       VkPipelineBindPoint pipelineBindPoint,
                       VkPipeline pipeline) {
  DEBUG_LAYER(vkCmdBindPipeline);
  next(commandBuffer, pipelineBindPoint, pipeline);
  AddCommand(commandBuffer,
             std::make_unique<CmdBindPipeline>(pipelineBindPoint, pipeline));
}

void vkCmdBindVertexBuffers(PFN_vkCmdBindVertexBuffers next,
                            VkCommandBuffer commandBuffer,
                            uint32_t firstBinding, uint32_t bindingCount,
                            VkBuffer const *pBuffers,
                            VkDeviceSize const *pOffsets) {
  DEBUG_LAYER(vkCmdBindVertexBuffers);
  next(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
  AddCommand(commandBuffer,
             std::make_unique<CmdBindVertexBuffers>(firstBinding, bindingCount,
                                                    pBuffers, pOffsets));
}

void vkCmdCopyBuffer(PFN_vkCmdCopyBuffer next, VkCommandBuffer commandBuffer,
                     VkBuffer srcBuffer, VkBuffer dstBuffer,
                     uint32_t regionCount, VkBufferCopy const *pRegions) {
  DEBUG_LAYER(vkCmdCopyBuffer);
  next(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
  AddCommand(commandBuffer, std::make_unique<CmdCopyBuffer>(
                                srcBuffer, dstBuffer, regionCount, pRegions));
}

void vkCmdDraw(PFN_vkCmdDraw next, VkCommandBuffer commandBuffer,
               uint32_t vertexCount, uint32_t instanceCount,
               uint32_t firstVertex, uint32_t firstInstance) {
  DEBUG_LAYER(vkCmdDraw);
  next(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
  AddCommand(commandBuffer,
             std::make_unique<CmdDraw>(vertexCount, instanceCount, firstVertex,
                                       firstInstance));
}

void vkCmdDrawIndexed(PFN_vkCmdDrawIndexed next, VkCommandBuffer commandBuffer,
                      uint32_t indexCount, uint32_t instanceCount,
                      uint32_t firstIndex, int32_t vertexOffset,
                      uint32_t firstInstance) {
  DEBUG_LAYER(vkCmdDrawIndexed);
  next(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset,
       firstInstance);
  AddCommand(commandBuffer, std::make_unique<CmdDrawIndexed>(
                                indexCount, instanceCount, firstIndex,
                                vertexOffset, firstInstance));
}

VkResult vkCreateBuffer(PFN_vkCreateBuffer next, VkDevice device,
                        VkBufferCreateInfo const *pCreateInfo,
                        AllocationCallbacks pAllocator, VkBuffer *pBuffer) {
  DEBUG_LAYER(vkCreateBuffer);
  auto result = next(device, pCreateInfo, pAllocator, pBuffer);
  if (result == VK_SUCCESS) {
    buffers.insert({*pBuffer, DeepCopy(*pCreateInfo)});
  }
  return result;
}

VkResult vkCreateDescriptorSetLayout(
    PFN_vkCreateDescriptorSetLayout next, VkDevice device,
    VkDescriptorSetLayoutCreateInfo const *pCreateInfo,
    AllocationCallbacks pAllocator, VkDescriptorSetLayout *pSetLayout) {
  DEBUG_LAYER(vkCreateDescriptorSetLayout);
  auto result = next(device, pCreateInfo, pAllocator, pSetLayout);
  if (result == VK_SUCCESS) {
    descriptorSetLayouts.insert({*pSetLayout, DeepCopy(*pCreateInfo)});
  }
  return result;
}

VkResult vkCreateFramebuffer(PFN_vkCreateFramebuffer next, VkDevice device,
                             VkFramebufferCreateInfo const *pCreateInfo,
                             AllocationCallbacks pAllocator,
                             VkFramebuffer *pFramebuffer) {
  DEBUG_LAYER(vkCreateFramebuffer);
  auto result = next(device, pCreateInfo, pAllocator, pFramebuffer);
  if (result == VK_SUCCESS) {
    framebuffers.insert({*pFramebuffer, DeepCopy(*pCreateInfo)});
  }
  return result;
}

VkResult vkCreateGraphicsPipelines(
    PFN_vkCreateGraphicsPipelines next, VkDevice device,
    VkPipelineCache pipelineCache, uint32_t createInfoCount,
    VkGraphicsPipelineCreateInfo const *pCreateInfos,
    AllocationCallbacks pAllocator, VkPipeline *pPipelines) {
  DEBUG_LAYER(vkCreateGraphicsPipelines);
  auto result = next(device, pipelineCache, createInfoCount, pCreateInfos,
                     pAllocator, pPipelines);
  if (result == VK_SUCCESS) {
    for (uint32_t i = 0; i < createInfoCount; i++) {
      graphicsPipelines.insert({pPipelines[i], DeepCopy(pCreateInfos[i])});
    }
  }
  return result;
}

VkResult vkCreatePipelineLayout(PFN_vkCreatePipelineLayout next,
                                VkDevice device,
                                VkPipelineLayoutCreateInfo const *pCreateInfo,
                                AllocationCallbacks pAllocator,
                                VkPipelineLayout *pPipelineLayout) {
  auto result = next(device, pCreateInfo, pAllocator, pPipelineLayout);
  if (result == VK_SUCCESS) {
    pipelineLayouts.insert({*pPipelineLayout, DeepCopy(*pCreateInfo)});
  }
  return result;
}

VkResult vkCreateRenderPass(PFN_vkCreateRenderPass next, VkDevice device,
                            VkRenderPassCreateInfo const *pCreateInfo,
                            AllocationCallbacks pAllocator,
                            VkRenderPass *pRenderPass) {
  DEBUG_LAYER(vkCreateRenderPass);
  auto result = next(device, pCreateInfo, pAllocator, pRenderPass);
  if (result == VK_SUCCESS) {
    renderPasses.insert({*pRenderPass, DeepCopy(*pCreateInfo)});
  }
  return result;
}

VkResult vkCreateShaderModule(PFN_vkCreateShaderModule next, VkDevice device,
                              VkShaderModuleCreateInfo const *pCreateInfo,
                              AllocationCallbacks pAllocator,
                              VkShaderModule *pShaderModule) {
  DEBUG_LAYER(vkCreateShaderModule);
  auto result = next(device, pCreateInfo, pAllocator, pShaderModule);
  if (result == VK_SUCCESS) {
    shaderModules.insert({*pShaderModule, DeepCopy(*pCreateInfo)});
  }
  return result;
}

VkResult vkMapMemory(PFN_vkMapMemory next, VkDevice device,
                     VkDeviceMemory memory, VkDeviceSize offset,
                     VkDeviceSize size, VkMemoryMapFlags flags, void **ppData) {
  DEBUG_LAYER(vkMapMemory);
  auto result = next(device, memory, offset, size, flags, ppData);
  if (result == VK_SUCCESS) {
    mappedMemory.insert({memory, {offset, size, flags, *ppData}});
  }
  return result;
}

struct BufferCopy {
  VkBuffer srcBuffer{};
  VkBuffer dstBuffer{};
  std::vector<std::shared_ptr<VkBufferCopy>> regions;
};

struct IndexBufferBinding {
  VkBuffer buffer;
  VkDeviceSize offset;
  VkIndexType indexType;
};

struct DrawCallStateTracker {
  bool graphicsPipelineIsBound = false;
  VkPipeline boundGraphicsPipeline = nullptr;
  VkRenderPassBeginInfo *currentRenderPass = nullptr;
  uint32_t currentSubpass = 0;
  std::unordered_map<uint32_t, VkDescriptorSet> boundGraphicsDescriptorSets;

  // TODO: also track buffer offsets
  std::unordered_map<uint32_t, VkBuffer> boundVertexBuffers;

  IndexBufferBinding boundIndexBuffer = {};
};

uint32_t getComponentWidth(VkFormat vkFormat) {
  switch (vkFormat) {
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
    return 4;
  default:
    assert(false && "Unknown format.");
  }
  // TODO: implement other formats
  return 0;
}

uint32_t getComponentCount(VkFormat vkFormat) {
  switch (vkFormat) {
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
    return 4;
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
    return 3;
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:
    return 2;
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
    return 1;
  default:
    assert(false && "Unknown format.");
  }
  // TODO: implement other formats
  return 0;
}

std::string getFormatTypeName(VkFormat vkFormat) {
  switch (vkFormat) {
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32_SFLOAT:
    return "float";
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32_UINT:
    return "uint32";
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32_SINT:
    return "int32";
  default:
    assert(false && "Unknown format.");
  }
  // TODO: implement other formats
}

std::string getBufferTypeName(VkFormat vkFormat) {
  auto componentCount = getComponentCount(vkFormat);
  if (componentCount == 1) {
    return getFormatTypeName(vkFormat);
  }

  std::stringstream strStream;
  strStream << "vec" << componentCount << "<" << getFormatTypeName(vkFormat)
            << ">";
  return strStream.str();
}

FormatType getFormatTypeCode(VkFormat vkFormat) {
  switch (vkFormat) {
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32_SFLOAT:
    return tFloat;
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32_UINT:
    return tUint32;
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32_SINT:
    return tInt32;
  default:
    assert(false && "Unknown format.");
  }
}

void HandleDrawCall(const DrawCallStateTracker &drawCallStateTracker,
                    uint32_t indexCount = 0);

VkResult vkQueueSubmit(PFN_vkQueueSubmit next, VkQueue queue,
                       uint32_t submitCount, VkSubmitInfo const *pSubmits,
                       VkFence fence) {
  DEBUG_LAYER(vkQueueSubmit);

  for (uint32_t submitIndex = 0; submitIndex < submitCount; submitIndex++) {
    for (uint32_t commandBufferIndex = 0;
         commandBufferIndex < pSubmits[submitIndex].commandBufferCount;
         commandBufferIndex++) {
      auto command_buffer =
          pSubmits[submitIndex].pCommandBuffers[commandBufferIndex];

      DrawCallStateTracker drawCallStateTracker = {};

      if (!command_buffers.count(command_buffer)) {
        continue;
      }
      for (auto &cmd : command_buffers.at(command_buffer)) {
        if (auto cmdBeginRenderPass = cmd->AsBeginRenderPass()) {
          drawCallStateTracker.currentRenderPass =
              cmdBeginRenderPass->pRenderPassBegin_;
          drawCallStateTracker.currentSubpass = 0;
        } else if (auto cmdBindDescriptorSets = cmd->AsBindDescriptorSets()) {
          if (cmdBindDescriptorSets->pipelineBindPoint_ ==
              VK_PIPELINE_BIND_POINT_GRAPHICS) {
            for (uint32_t descriptorSetOffset = 0;
                 descriptorSetOffset <
                 cmdBindDescriptorSets->descriptorSetCount_;
                 descriptorSetOffset++) {
              drawCallStateTracker.boundGraphicsDescriptorSets.insert(
                  {cmdBindDescriptorSets->firstSet_ + descriptorSetOffset,
                   cmdBindDescriptorSets
                       ->pDescriptorSets_[descriptorSetOffset]});
            }
          }
        } else if (auto cmdBindIndexBuffer = cmd->AsBindIndexBuffer()) {
          drawCallStateTracker.boundIndexBuffer.buffer =
              cmdBindIndexBuffer->buffer_;
          drawCallStateTracker.boundIndexBuffer.offset =
              cmdBindIndexBuffer->offset_;
          drawCallStateTracker.boundIndexBuffer.indexType =
              cmdBindIndexBuffer->indexType_;
        } else if (auto cmdBindPipeline = cmd->AsBindPipeline()) {
          switch (cmdBindPipeline->pipelineBindPoint_) {
          case VK_PIPELINE_BIND_POINT_GRAPHICS:
            drawCallStateTracker.graphicsPipelineIsBound = true;
            drawCallStateTracker.boundGraphicsPipeline =
                cmdBindPipeline->pipeline_;
            break;
          default:
            // Not considering other pipelines now.
            break;
          }
        } else if (auto cmdBindVertexBuffers = cmd->AsBindVertexBuffers()) {
          for (uint32_t bindingOffset = 0;
               bindingOffset < cmdBindVertexBuffers->bindingCount_;
               bindingOffset++) {
            drawCallStateTracker.boundVertexBuffers.insert(
                {bindingOffset + cmdBindVertexBuffers->firstBinding_,
                 cmdBindVertexBuffers
                     ->pBuffers_[bindingOffset +
                                 cmdBindVertexBuffers->firstBinding_]});
          }
        } else if (auto cmdCopyBuffer = cmd->AsCopyBuffer()) {
          auto bufferCopy = std::make_shared<BufferCopy>();
          bufferCopy->dstBuffer = cmdCopyBuffer->dstBuffer_;
          bufferCopy->srcBuffer = cmdCopyBuffer->srcBuffer_;
          for (uint32_t regionIdx = 0; regionIdx < cmdCopyBuffer->regionCount_;
               regionIdx++) {

            auto copyRegion = std::make_shared<VkBufferCopy>(
                cmdCopyBuffer->pRegions_[regionIdx]);
            bufferCopy->regions.push_back(copyRegion);
          }
          bufferCopies.push_back(bufferCopy);
        } else if (auto cmdDraw = cmd->AsDraw()) {
          HandleDrawCall(drawCallStateTracker);
        } else if (auto cmdDrawIndexed = cmd->AsDrawIndexed()) {
          HandleDrawCall(drawCallStateTracker, cmdDrawIndexed->indexCount_);
        } else {
          assert(false && "Unknown command.");
        }
      }
    }
  }
  return next(queue, submitCount, pSubmits, fence);
}

void vkUpdateDescriptorSets(PFN_vkUpdateDescriptorSets next, VkDevice device,
                            uint32_t descriptorWriteCount,
                            VkWriteDescriptorSet const *pDescriptorWrites,
                            uint32_t descriptorCopyCount,
                            VkCopyDescriptorSet const *pDescriptorCopies) {
  DEBUG_LAYER(vkUpdateDescriptorSets);
  next(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount,
       pDescriptorCopies);
  assert(descriptorCopyCount == 0 && "Not handling descriptor copies yet.");
  for (uint32_t i = 0; i < descriptorWriteCount; i++) {
    auto writeDescriptorSet = pDescriptorWrites[i];
    assert(writeDescriptorSet.dstArrayElement == 0);
    assert(writeDescriptorSet.descriptorCount == 1);

    switch (writeDescriptorSet.descriptorType) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      // pImageInfo must be a valid pointer to an array of descriptorCount valid
      // VkDescriptorImageInfo structures
      break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      // pTexelBufferView must be a valid pointer to an array of descriptorCount
      // valid VkBufferView handles
      break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
      // pBufferInfo must be a valid pointer to an array of descriptorCount
      // valid VkDescriptorBufferInfo structures
      if (!descriptorSetToBindingBuffer.count(writeDescriptorSet.dstSet)) {
        descriptorSetToBindingBuffer.insert({writeDescriptorSet.dstSet, {}});
      }
      auto &bindingToBuffer =
          descriptorSetToBindingBuffer.at(writeDescriptorSet.dstSet);
      bindingToBuffer.insert(
          {writeDescriptorSet.dstBinding, writeDescriptorSet.pBufferInfo[0]});
      break;
    }
    default:
      assert(false && "Should be unreachable.");
      break;
    }
  }
}

void HandleDrawCall(const DrawCallStateTracker &drawCallStateTracker,
                    uint32_t indexCount) {
  if (!drawCallStateTracker.graphicsPipelineIsBound) {
    return;
  }

  assert(drawCallStateTracker.currentRenderPass);

  VkShaderModule vertexShader = nullptr;
  VkShaderModule fragmentShader = nullptr;
  auto graphicsPipelineCreateInfo =
      graphicsPipelines.at(drawCallStateTracker.boundGraphicsPipeline);
  for (uint32_t stageIndex = 0;
       stageIndex < graphicsPipelineCreateInfo.stageCount; stageIndex++) {
    auto stageCreateInfo = graphicsPipelineCreateInfo.pStages[stageIndex];
    if (stageCreateInfo.stage == VK_SHADER_STAGE_VERTEX_BIT) {
      vertexShader = stageCreateInfo.module;
    } else if (stageCreateInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      fragmentShader = stageCreateInfo.module;
    } else {
      assert(false && "Not handled.");
    }
  }
  assert(vertexShader && "Vertex shader required for graphics pipeline.");
  assert(fragmentShader && "Fragment shader required for graphics pipeline.");

  std::cout << "#!amber" << std::endl << std::endl;

  std::cout << "SHADER vertex vertex_shader SPIRV-ASM" << std::endl;
  std::cout << GetDisassembly(vertexShader);
  std::cout << "END" << std::endl << std::endl;

  std::cout << "SHADER fragment fragment_shader SPIRV-ASM" << std::endl;
  std::cout << GetDisassembly(fragmentShader);
  std::cout << "END" << std::endl << std::endl;

  std::cout << "# Shaders for creating a 2x2 texture." << std::endl;
  std::cout << "SHADER vertex vert_shader PASSTHROUGH" << std::endl;
  std::cout << "SHADER fragment frag_shader_red GLSL" << std::endl
            << "#version 430" << std::endl
            << "layout(location = 0) out vec4 color_out;" << std::endl
            << "void main() {" << std::endl
            << "  color_out = vec4(1.0, 0.0, 0.0, 1.0);" << std::endl
            << "}" << std::endl
            << "END" << std::endl
            << std::endl;

  std::cout << "BUFFER texture FORMAT R8G8B8A8_UNORM" << std::endl
            << "SAMPLER sampler" << std::endl
            << std::endl;

  std::cout << "PIPELINE graphics texture_create_pipeline\n"
               "  ATTACH vert_shader\n"
               "  ATTACH frag_shader_red\n"
               "  FRAMEBUFFER_SIZE 2 2\n"
               "  BIND BUFFER texture AS color LOCATION 0\n"
               "END\n";

  std::stringstream bufferDeclarationStringStream;
  std::stringstream descriptorSetBindingStringStream;
  std::stringstream framebufferAttachmentStringStream;
  std::stringstream graphicsPipelineStringStream;

  for (auto vertexBufferBinding : drawCallStateTracker.boundVertexBuffers) {
    auto buffer = buffers.at(vertexBufferBinding.second);
    assert(buffer.usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    auto bindingDescription =
        graphicsPipelineCreateInfo.pVertexInputState
            ->pVertexBindingDescriptions[vertexBufferBinding.first];

    assert(bindingDescription.inputRate == VK_VERTEX_INPUT_RATE_VERTEX &&
           "inputRate not implemented");

    if (bufferToMemory.count(vertexBufferBinding.second)) {

      VkBuffer vertexBuffer = nullptr;

      // Check if a staging buffer is used.
      for (auto bufferCopy : bufferCopies) {
        if (bufferCopy->dstBuffer != vertexBufferBinding.second)
          continue;
        vertexBuffer = bufferCopy->srcBuffer;
        break;
      }

      // Assume that the bound vertex buffer contains the data if a staging
      // buffer is not found.
      if (vertexBuffer == nullptr) {
        vertexBuffer = vertexBufferBinding.second;
      }

      VkDeviceMemory deviceMemory = bufferToMemory.at(vertexBuffer).first;

      assert(mappedMemory.count(deviceMemory) &&
             "Vertex buffer mapping not found");

      auto memoryMapping = mappedMemory.at(deviceMemory);

      void *bufferPtr = std::get<3>(memoryMapping);

      // Create string stream for every location
      std::vector<std::shared_ptr<std::stringstream>> bufferDeclStrings;
      for (uint32_t location = 0;
           location < graphicsPipelineCreateInfo.pVertexInputState
                          ->vertexAttributeDescriptionCount;
           location++) {

        auto description = graphicsPipelineCreateInfo.pVertexInputState
                               ->pVertexAttributeDescriptions[location];

        std::stringstream bufferName;
        bufferName << "vert_" << vertexBufferBinding.first << "_" << location;

        graphicsPipelineStringStream << "  VERTEX_DATA " << bufferName.str()
                                     << " LOCATION " << location << std::endl;

        auto strStream = std::make_shared<std::stringstream>();
        *strStream << "BUFFER " << bufferName.str() << " DATA_TYPE "
                   << getBufferTypeName(description.format) << " DATA "
                   << std::endl
                   << "  ";
        bufferDeclStrings.push_back(strStream);
      }

      // Go through all elements in the buffer
      for (uint32_t bufferOffset = 0; bufferOffset < buffer.size;
           bufferOffset += bindingDescription.stride) {

        auto readPtr = (char *)bufferPtr + bufferOffset;

        for (uint32_t location = 0;
             location < graphicsPipelineCreateInfo.pVertexInputState
                            ->vertexAttributeDescriptionCount;
             location++) {
          auto description = graphicsPipelineCreateInfo.pVertexInputState
                                 ->pVertexAttributeDescriptions[location];

          uint32_t components = getComponentCount(description.format);
          uint32_t componentWidth = getComponentWidth(description.format);

          for (uint32_t component = 0; component < components; component++) {
            switch (getFormatTypeCode(description.format)) {
            case tFloat:
              *bufferDeclStrings.at(location) << *(float *)readPtr << " ";
              break;
            case tInt32:
              *bufferDeclStrings.at(location) << *(int32_t *)readPtr << " ";
              break;
            case tUint32:
              *bufferDeclStrings.at(location) << *(uint32_t *)readPtr << " ";
              break;
            // TODO: implement other formats
            default:
              assert(false && "Unimplemented format");
            }

            readPtr += componentWidth;
          }
        }
      }

      // End all buffer declaration streams and combine them to one stream.
      for (auto stream : bufferDeclStrings) {
        *stream << std::endl << "END" << std::endl << std::endl;
        bufferDeclarationStringStream << (*stream).str();
      }
    } else {
      bufferDeclarationStringStream << "...";
    }
  }

  // Declare index buffer (if used)
  if (indexCount > 0) {

    auto buffer = buffers.at(drawCallStateTracker.boundIndexBuffer.buffer);
    assert(buffer.usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    assert(bufferToMemory.count(drawCallStateTracker.boundIndexBuffer.buffer) &&
           "Index buffer memory not found");

    VkBuffer indexBuffer = nullptr;
    VkDeviceSize bufferCopyOffset = 0;
    // Check if a staging buffer is used.
    for (const auto &bufferCopy : bufferCopies) {
      if (bufferCopy->dstBuffer ==
          drawCallStateTracker.boundIndexBuffer.buffer) {
        indexBuffer = bufferCopy->srcBuffer;
        assert(bufferCopy->regions.size() == 1 &&
               "Only one copy region supported.");
        bufferCopyOffset = bufferCopy->regions.at(0)->srcOffset;
        break;
      }
    }

    // Assume that the bound index buffer contains the data if a staging
    // buffer is not found.
    if (indexBuffer == nullptr) {
      indexBuffer = drawCallStateTracker.boundIndexBuffer.buffer;
    }

    graphicsPipelineStringStream << "  INDEX_DATA index_buffer" << std::endl;

    // Amber supports only 32 bit indices. 16 bit indices will be used as 32
    // bit.
    bufferDeclarationStringStream << "BUFFER index_buffer DATA_TYPE uint32 ";

    bufferDeclarationStringStream << "DATA " << std::endl << "  ";

    VkDeviceMemory deviceMemory = bufferToMemory.at(indexBuffer).first;
    assert(mappedMemory.count(deviceMemory) &&
           "Index buffer mapping not found");
    auto memoryMapping = mappedMemory.at(deviceMemory);
    void *bufferPtr = (char *)std::get<3>(memoryMapping) + bufferCopyOffset +
                      drawCallStateTracker.boundIndexBuffer.offset;

    // 16-bit indices
    if (drawCallStateTracker.boundIndexBuffer.indexType ==
        VK_INDEX_TYPE_UINT16) {
      auto ptr = (uint16_t *)bufferPtr;
      for (uint32_t indexIdx = 0; indexIdx < indexCount; indexIdx++) {
        bufferDeclarationStringStream << ptr[indexIdx] << " ";
      }
    }
    // 32-bit indices
    else if (drawCallStateTracker.boundIndexBuffer.indexType ==
             VK_INDEX_TYPE_UINT32) {
      auto ptr = (uint32_t *)bufferPtr;
      for (uint32_t indexIdx = 0; indexIdx < indexCount; indexIdx++) {
        bufferDeclarationStringStream << ptr[indexIdx] << " ";
      }
    } else {
      assert(false && "Invalid indexType");
    }

    bufferDeclarationStringStream << std::endl
                                  << "END" << std::endl
                                  << std::endl;
  }

  for (auto descriptorSet : drawCallStateTracker.boundGraphicsDescriptorSets) {

    uint32_t descriptorSetNumber = descriptorSet.first;
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo =
        descriptorSetLayouts.at(descriptorSets.at(descriptorSet.second));

    auto bindingAndBuffers =
        descriptorSetToBindingBuffer.at(descriptorSet.second);
    for (auto bindingAndBuffer : bindingAndBuffers) {

      uint32_t bindingNumber = bindingAndBuffer.first;
      VkDescriptorBufferInfo bufferInfo = bindingAndBuffer.second;

      std::stringstream strstr;
      strstr << "buf_" << descriptorSetNumber << "_" << bindingNumber;
      std::string bufferName = strstr.str();

      VkBufferCreateInfo bufferCreateInfo = buffers.at(bufferInfo.buffer);
      assert(bufferInfo.offset == 0);

      bufferDeclarationStringStream << "BUFFER " << bufferName << " DATA_TYPE "
                                    << "float"
                                    << " DATA" << std::endl;
      bufferDeclarationStringStream << "  ";

      if (bufferToMemory.count(bufferInfo.buffer)) {
        VkDeviceMemory deviceMemory =
            bufferToMemory.at(bufferInfo.buffer).first;
        if (mappedMemory.count(deviceMemory)) {
          std::tuple<VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void *>
              &memoryMappingEntry = mappedMemory.at(deviceMemory);
          VkDeviceSize range = bufferInfo.range == VK_WHOLE_SIZE
                                   ? bufferCreateInfo.size
                                   : bufferInfo.range;
          assert(std::get<0>(memoryMappingEntry) == 0);
          assert(std::get<1>(memoryMappingEntry) >= range);
          auto *thePtr = (float *)std::get<3>(memoryMappingEntry);
          for (int bidx = 0; bidx < range / sizeof(float); bidx++) {
            if (bidx > 0) {
              bufferDeclarationStringStream << " ";
            }
            bufferDeclarationStringStream << thePtr[bidx];
          }
        }
      } else {
        bufferDeclarationStringStream << "...";
      }
      bufferDeclarationStringStream << std::endl;
      bufferDeclarationStringStream << "END" << std::endl << std::endl;

      VkDescriptorSetLayoutBinding layoutBinding =
          layoutCreateInfo.pBindings[bindingNumber];
      VkDescriptorType descriptorType = layoutBinding.descriptorType;
      descriptorSetBindingStringStream
          << "  BIND BUFFER " << bufferName << " AS "
          << (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? "uniform"
                                                                  : "...")
          << " DESCRIPTOR_SET " << descriptorSetNumber << " BINDING "
          << bindingNumber << std::endl;
    }
  }

  VkRenderPassCreateInfo renderPassCreateInfo =
      renderPasses.at(drawCallStateTracker.currentRenderPass->renderPass);
  for (uint colorAttachment = 0;
       colorAttachment <
       renderPassCreateInfo.pSubpasses[drawCallStateTracker.currentSubpass]
           .colorAttachmentCount;
       colorAttachment++) {
    bufferDeclarationStringStream << "BUFFER framebuffer_" << colorAttachment
                                  << " FORMAT B8G8R8A8_UNORM" << std::endl
                                  << std::endl;
    framebufferAttachmentStringStream
        << "  BIND BUFFER framebuffer_" << colorAttachment
        << " AS color LOCATION " << colorAttachment << std::endl;
  }

  std::cout << bufferDeclarationStringStream.str();

  std::cout << "PIPELINE graphics pipeline" << std::endl;
  std::cout << "  ATTACH vertex_shader" << std::endl;
  std::cout << "  ATTACH fragment_shader" << std::endl;
  VkFramebufferCreateInfo framebufferCreateInfo =
      framebuffers.at(drawCallStateTracker.currentRenderPass->framebuffer);
  std::cout << "  FRAMEBUFFER_SIZE " << framebufferCreateInfo.width << " "
            << framebufferCreateInfo.height << std::endl;
  std::cout << framebufferAttachmentStringStream.str();
  std::cout << descriptorSetBindingStringStream.str();

  // Add definitions for pipeline
  std::cout << graphicsPipelineStringStream.str();

  std::cout
      << "  BIND SAMPLER sampler DESCRIPTOR_SET 0 BINDING 1\n"
         "  BIND BUFFER texture AS sampled_image DESCRIPTOR_SET 0 BINDING 2\n";

  std::cout << "END" << std::endl << std::endl; // PIPELINE

  std::cout << "CLEAR_COLOR pipeline 0 0 0 255" << std::endl;
  std::cout << std::endl;

  std::cout
      << "# Generate a 2x2 texture with a one pixel sized chessboard pattern.\n"
         "CLEAR_COLOR texture_create_pipeline 0 0 255 255\n"
         "CLEAR texture_create_pipeline\n"
         "RUN texture_create_pipeline DRAW_RECT POS 0 0 SIZE 1 1\n"
         "RUN texture_create_pipeline DRAW_RECT POS 1 1 SIZE 1 1\n"
      << std::endl;

  std::cout << "CLEAR pipeline" << std::endl;

  std::string topology =
      topologies.at(graphicsPipelineCreateInfo.pInputAssemblyState->topology);

  if (indexCount > 0) {
    std::cout << "RUN pipeline DRAW_ARRAY AS " << topology << " INDEXED"
              << std::endl;
  } else {
    std::cout << "RUN pipeline DRAW_ARRAY AS " << topology << std::endl;
  }

  exit(0);
} // namespace graphicsfuzz_amber_scoop

} // namespace graphicsfuzz_amber_scoop