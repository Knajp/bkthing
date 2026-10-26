#ifndef SHADER_H
#define SHADER_H

#include <vector>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <string>
#include <cstdint>
#include <iostream>
#include <spirv-tools/libspirv.h>
#include <spirv-tools/optimizer.hpp>
#include <fstream>
#include <sstream>
#include <volk/volk.h>
#include <vulkan/vulkan.h>
#include "../Logger.hpp"

namespace shader
{
  class ShaderTool
  {
  public:
    static VkShaderModule createShaderModule(const std::string& path, EShLanguage stage, VkDevice device)
    {
      VkShaderModule shaderModule;
      
      std::vector<uint32_t> code = optimizeSPIRV(GLSLtoSPIRV(readFile(path), stage));
      VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size() * sizeof(uint32_t),
        .pCode = code.data()
      };

      if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
      {
        spdlog::error("Failed to create shader module!");
        throw std::runtime_error("err");
      }

      return shaderModule;
    }
    static std::string readFile(const std::string& path)
    {
      std::ifstream file(path);
      if(!file)
        std::cerr << "Failed to open file for read: " << path << "\n";

      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
    }
    static std::vector<uint32_t> GLSLtoSPIRV(const std::string& glslSource, EShLanguage stage)
    {
      glslang::InitializeProcess();

      const char* sourceCstr = glslSource.c_str();
      glslang::TShader shader(stage);
      shader.setStrings(&sourceCstr, 1);

      constexpr int glslVersion = 450;
      shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, glslVersion);
      shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_4);
      shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

      const TBuiltInResource* resource = GetDefaultResources();
      EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);
    
      if(!shader.parse(resource, glslVersion, false, messages))
      {
        std::cerr << "GLSL parsing failed:\n" << shader.getInfoLog() << "\n" << shader.getInfoDebugLog();
        glslang::FinalizeProcess();
        return {};
      }
      
      glslang::TProgram program;
      program.addShader(&shader);
  
      if(!program.link(messages))
      {
        std::cerr << "GLSL linking failed:\n" << program.getInfoLog();
        glslang::FinalizeProcess();
        return {};
      }
      
      std::vector<uint32_t> spirvBinary;
      spv::SpvBuildLogger logger;
      glslang::SpvOptions spvOptions;

      spvOptions.generateDebugInfo = false;
      spvOptions.disableOptimizer = true;

      glslang::GlslangToSpv(*program.getIntermediate(stage), spirvBinary, &logger, &spvOptions);
      glslang::FinalizeProcess();
      
      return spirvBinary;
    }
    
    static std::vector<uint32_t> optimizeSPIRV(const std::vector<uint32_t>& inputSpirv)
    {
      spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_4);
      
      optimizer.RegisterPerformancePasses();

      std::vector<uint32_t> optimizedSpirv;
      if(!optimizer.Run(inputSpirv.data(), inputSpirv.size(), &optimizedSpirv))
      {
        std::cerr << "SPIR-V optimization failed!\n";
        return inputSpirv;
      }
      
      return optimizedSpirv;
    }
  private:
    ShaderTool() = default;
  };
}
#endif 
