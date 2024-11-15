#include <ili/runtime.hpp>
#include <ili/opcodes.hpp>

#include <fmt/format.h>

namespace ili {
    i32 Runtime::run(Executable&& executable) {
        auto &newExecutable = this->m_executables.emplace("", std::move(executable)).first->second;
        m_stack = Stack(newExecutable.getStackSize());

        auto &method = m_methodStack.emplace_back(Method(&newExecutable, newExecutable.getEntrypointMethodToken()));

        executeInstructions(method);

        return 0;
    }

    std::generator<op::Instruction> Method::getInstructions() {
        const auto bytes = getByteCode();
        while (m_instructionOffset < bytes.size()) {
            const auto instruction = op::Instruction(bytes.subspan(m_instructionOffset));
            m_instructionOffset += instruction.getLength();

            co_yield instruction;
        }

        co_return;
    }


    Method::Method(const Executable *executable, table::Token methodToken) : m_executable(executable), m_methodToken(methodToken) {

    }

    const table::MethodDef* Method::getMethodDef() const {
        if (m_methodDef != nullptr) return m_methodDef;

        m_methodDef = m_executable->getTableEntry<table::MethodDef>(m_methodToken);

        return m_methodDef;
    }

    table::Token Method::getToken() const {
        return m_methodToken;
    }

    std::span<const u8> Method::getByteCode() const {
        if (!m_byteCode.empty()) return m_byteCode;

        const auto methodDef = getMethodDef();
        const auto section = m_executable->getVirtualSection(methodDef->rva);
        if (section == nullptr) {
            return {};
        }

        // Try to interpret the data using the tiny header format
        {
            const auto tinyHeaderBytes = m_executable->getSectionBytes(*section, methodDef->rva, sizeof(CorILMethodTiny));
            const auto tinyHeader = reinterpret_cast<const CorILMethodTiny *>(tinyHeaderBytes.data());

            if (CorILMethodType(tinyHeader->type) == CorILMethodType::TinyFormat) {
                m_byteCode = m_executable->getSectionBytes(*section, methodDef->rva + sizeof(CorILMethodTiny), tinyHeader->size);
            }
        }

        // If that didn't work, it must be a fat header
        if (m_byteCode.empty()) {
            const auto fatHeaderBytes = m_executable->getSectionBytes(*section, methodDef->rva, sizeof(CorILMethodFat));
            const auto fatHeader = reinterpret_cast<const CorILMethodFat *>(fatHeaderBytes.data());

            if (CorILMethodType(fatHeader->type) == CorILMethodType::FatFormat) {
                m_byteCode = m_executable->getSectionBytes(*section, methodDef->rva + fatHeader->headerSize * sizeof(u32), fatHeader->codeSize);
            }
        }

        // If that still didn't work, the data is bogus
        if (m_byteCode.empty()) {
            throw std::runtime_error(fmt::format("Cannot get byte code of method {}", m_methodToken.getIndex()));
        }

        return m_byteCode;
    }




}
