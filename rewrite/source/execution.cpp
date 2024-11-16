#include <csignal>
#include <ili/runtime.hpp>

#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

namespace ili {

    void Runtime::nop() {
        asm volatile("nop");
    }

    void Runtime::brk() {
        ::raise(SIGILL);
    }

    void Runtime::call(Method &method, table::Token token) {
        switch (token.getId()) {
            case table::MethodDef::ID: {
                const auto &executable = method.getExecutable();
                auto methodToCall = Method(executable, token);

                const auto methodExecutable = methodToCall.getExecutable();
                const auto methodDef = method.getMethodDef();
                const auto typeDef = executable->getTypeDefOfMethod(methodDef);
                const auto module = executable->getModule();

                const auto moduleName = methodExecutable->getString(module->nameIndex);
                const auto namespaceName = methodExecutable->getString(typeDef->typeNamespaceIndex);
                const auto typeName = methodExecutable->getString(typeDef->typeNameIndex);
                const auto methodName = methodExecutable->getString(methodDef->nameIndex);

                fmt::println("Executing .NET method '[{}]{}::{}::{}'",
                    moduleName, namespaceName, typeName, methodName
                );

                executeInstructions(methodToCall);
                break;
            }
            case table::MemberRef::ID: {
                const auto qualifiedMethodName = method.getExecutable()->getQualifiedMemberName(token);
                const auto assemblyName = std::string(qualifiedMethodName.assemblyName);

                auto assemblyIt = m_assemblies.find(assemblyName);
                if (assemblyIt == m_assemblies.end()) {
                    // Executable has not been loaded yet, try to load it.

                    for (const auto &loaderFunction : m_assemblyLoaders) {
                        auto result = loaderFunction(assemblyName);
                        if (!result.has_value())
                            continue;

                        assemblyIt = m_assemblies.emplace(assemblyName, std::move(result.value())).first;
                        break;
                    }
                }

                if (assemblyIt == m_assemblies.end()) {
                    throw std::runtime_error(fmt::format("Could not find assembly '{}'", assemblyName));
                }

                const auto &assembly = assemblyIt->second;
                const auto methodDef = assembly.getMethodByName(qualifiedMethodName.namespaceName, qualifiedMethodName.typeName, qualifiedMethodName.methodName);
                if (methodDef == nullptr) {
                    throw std::runtime_error(fmt::format("Assembly '{}' does not contain method '{}'", assemblyName, std::string(qualifiedMethodName)));
                }

                fmt::println("Executing .NET method '{}'", std::string(qualifiedMethodName));

                Method methodToCall(&assembly, assembly.getTokenOfTableEntry(methodDef));
                executeInstructions(methodToCall);

                break;
            }
            default: throw std::runtime_error("Invalid call token type");
        }
    }

    void Runtime::ldstr(Method &, u32 value) {
        m_stack.push<u64>(ValueType::O, value);
    }

    void Runtime::ldarg(Method &method, u16 id) {
        std::ignore = method;
        std::ignore = id;
    }

    void Runtime::stloc(Method& method, u16 id) {
        auto& localVariable = method.getLocalVariable(id);

        auto &stack = m_stack;
        const auto type = stack.getTypeOnStack();
        switch (type) {
            using enum ValueType;
            case Int32:               localVariable = std::make_unique<Variable<i32>>(type, stack.pop<i32>()); break;
            case Int64:               localVariable = std::make_unique<Variable<i64>>(type, stack.pop<i64>()); break;
            case Native_int:          localVariable = std::make_unique<Variable<i64>>(type, stack.pop<int>()); break;
            case Native_unsigned_int: localVariable = std::make_unique<Variable<u64>>(type, stack.pop<unsigned int>()); break;
            case F:                   localVariable = std::make_unique<Variable<f64>>(type, stack.pop<f64>()); break;
            case O:                   localVariable = std::make_unique<Variable<ManagedPointer>>(type, stack.pop<ManagedPointer>()); break;
            case Pointer:             localVariable = std::make_unique<Variable<UnmanagedPointer>>(type, stack.pop<UnmanagedPointer>()); break;
            case Invalid:             throw std::runtime_error("Invalid method token type");
        }
    }

    void Runtime::ldloc(Method& method, u16 id) {
        auto& localVariable = method.getLocalVariable(id);
        const auto type = localVariable->type;

        auto &stack = m_stack;
        switch (type) {
            using enum ValueType;
            case Int32:               stack.push<i32>(type, static_cast<Variable<i32>*>(localVariable.get())->value); break;
            case Int64:               stack.push<i64>(type, static_cast<Variable<i64>*>(localVariable.get())->value); break;
            case Native_int:          stack.push<i64>(type, static_cast<Variable<int>*>(localVariable.get())->value); break;
            case Native_unsigned_int: stack.push<u64>(type, static_cast<Variable<unsigned int>*>(localVariable.get())->value); break;
            case F:                   stack.push<f64>(type, static_cast<Variable<f64>*>(localVariable.get())->value); break;
            case O:                   stack.push<ManagedPointer>(type, static_cast<Variable<ManagedPointer>*>(localVariable.get())->value); break;
            case Pointer:             stack.push<UnmanagedPointer>(type, static_cast<Variable<UnmanagedPointer>*>(localVariable.get())->value); break;
            case Invalid:             throw std::runtime_error("Invalid method token type");
        }

        localVariable.reset();
    }

    void Runtime::br(Method &method, i32 offset) {
        method.offsetProgramCounter(offset);
    }


    void Runtime::ldloca(Method &method, u16 id) {
        m_stack.push(ValueType::Pointer, reinterpret_cast<u64>(method.getLocalVariable(id).get()));
    }

    void Runtime::executeInstructions(Method& method) {
        for (const auto instruction : method.getInstructions()) {
            fmt::println("{}", magic_enum::enum_name(instruction.getOpcode()));

            switch (instruction.getOpcode()) {
                using enum op::Opcode;
                case Nop:       nop();                                                            break;
                case Brk:       brk();                                                            break;
                case Call:      call(method, instruction.get<table::Token>(0));    break;
                case Ldstr:     ldstr(method, instruction.get<u32>(0));             break;
                case Ldloca_s:  ldloca(method, instruction.get<u8>(0));               break;
                case Ldarg_0:   ldarg(method, 0);                                           break;
                case Ldarg_1:   ldarg(method, 1);                                           break;
                case Ldarg_2:   ldarg(method, 2);                                           break;
                case Ldarg_3:   ldarg(method, 3);                                           break;
                case Ldarg:     ldarg(method, instruction.get<u8>(0));                break;
                case Ldarg_s:   ldarg(method, instruction.get<u16>(0));               break;
                case Stloc_0:   stloc(method, 0);                                           break;
                case Stloc_1:   stloc(method, 1);                                           break;
                case Stloc_2:   stloc(method, 2);                                           break;
                case Stloc_3:   stloc(method, 3);                                           break;
                case Stloc:     stloc(method, instruction.get<u8>(0));                break;
                case Stloc_s:   stloc(method, instruction.get<u16>(0));               break;
                case Ldloc_0:   ldloc(method, 0);                                           break;
                case Ldloc_1:   ldloc(method, 1);                                           break;
                case Ldloc_2:   ldloc(method, 2);                                           break;
                case Ldloc_3:   ldloc(method, 3);                                           break;
                case Ldloc:     ldloc(method, instruction.get<u8>(0));                break;
                case Ldloc_s:   ldloc(method, instruction.get<u16>(0));               break;
                case Ret:       return;
                case Br:        br(method, instruction.get<i32>(0));               break;
                case Br_s:      br(method, instruction.get<i8>(0));                break;
                default:
                    throw std::runtime_error(fmt::format("Unimplemented opcode {}", magic_enum::enum_name(instruction.getOpcode())));
            }
        }
    }

}