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
                const auto &executable = method.getAssembly();
                auto methodToCall = Method(executable, token);

                const auto methodExecutable = methodToCall.getAssembly();
                const auto methodDef = methodToCall.getMethodDef();
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
                const auto qualifiedMethodName = method.getAssembly()->getQualifiedMemberName(token);
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

    void Runtime::ldstr(Method &, table::Token value) {
        m_stack.push<ManagedPointer>(ManagedPointer(value.value));
    }

    void Runtime::ldarg(Method &method, u16 id) {
        std::ignore = method;
        std::ignore = id;
    }

    void Runtime::stloc(Method& method, u16 id) {
        auto& localVariable = method.getLocalVariable(id);

        localVariable = createVariableFromStackContent();
    }

    void Runtime::ldloc(Method& method, u16 id) {
        auto& localVariable = method.getLocalVariable(id);
        const auto type = localVariable->type;

        auto &stack = m_stack;
        switch (type) {
            using enum ValueType;
            case Int32:               stack.push<i32>(static_cast<Variable<i32>*>(localVariable.get())->value); break;
            case Int64:               stack.push<i64>(static_cast<Variable<i64>*>(localVariable.get())->value); break;
            case NativeInt:           stack.push<ili::NativeInt>(static_cast<Variable<ili::NativeInt>*>(localVariable.get())->value); break;
            case NativeUnsignedInt:   stack.push<ili::NativeUnsignedInt>(static_cast<Variable<ili::NativeUnsignedInt>*>(localVariable.get())->value); break;
            case F:                   stack.push<Float>(static_cast<Variable<Float>*>(localVariable.get())->value); break;
            case O:                   stack.push<ManagedPointer>(static_cast<Variable<ManagedPointer>*>(localVariable.get())->value); break;
            case Pointer:             stack.push<UnmanagedPointer>(static_cast<Variable<UnmanagedPointer>*>(localVariable.get())->value); break;
            case Invalid:             throw std::runtime_error("Invalid method token type");
        }

        localVariable.reset();
    }

    void Runtime::br(Method &method, i32 offset) {
        method.offsetProgramCounter(offset);
    }

    const table::Field* Runtime::loadStaticField(Method& method, table::Token token) {
        const auto assembly = method.getAssembly();
        const auto field = assembly->getTableEntry<table::Field>(token);

        const auto typeDef = assembly->getTypeDefOfField(field);

        // If containing type was not initialized yet, initialize it
        if (m_initializedTypes.insert(typeDef).second) {
            fmt::println("Initializing type '{}'", assembly->getString(typeDef->typeNameIndex));

            auto cctorMethod = assembly->getMethodOfType(typeDef, ".cctor");
            Method methodToCall(assembly, assembly->getTokenOfTableEntry(cctorMethod));
            executeInstructions(methodToCall);
        }

        fmt::println("Accessing field {}", assembly->getString(field->nameIndex));

        return field;
    }

    std::unique_ptr<VariableBase> Runtime::createVariableFromStackContent() {
        auto &stack = m_stack;
        const auto type = stack.getTypeOnStack();
        switch (type) {
            using enum ValueType;
            case Int32:               return std::make_unique<Variable<i32>>(type, stack.pop<i32>());
            case Int64:               return std::make_unique<Variable<i64>>(type, stack.pop<i64>());
            case NativeInt:           return std::make_unique<Variable<ili::NativeInt>>(type, stack.pop<ili::NativeInt>());
            case NativeUnsignedInt:   return std::make_unique<Variable<ili::NativeUnsignedInt>>(type, stack.pop<ili::NativeUnsignedInt>());
            case F:                   return std::make_unique<Variable<Float>>(type, stack.pop<Float>());
            case O:                   return std::make_unique<Variable<ManagedPointer>>(type, stack.pop<ManagedPointer>());
            case Pointer:             return std::make_unique<Variable<UnmanagedPointer>>(type, stack.pop<UnmanagedPointer>());
            case Invalid:
            default:                  throw  std::runtime_error("Invalid method token type");
        }
    }

    void Runtime::storeVariableOnStack(const std::unique_ptr<VariableBase> &variable) {
        const auto type = variable->type;
        auto &stack = m_stack;

        switch (type) {
            using enum ValueType;
            case Int32:               stack.push<i32>(static_cast<Variable<i32>*>(variable.get())->value); break;
            case Int64:               stack.push<i64>(static_cast<Variable<i64>*>(variable.get())->value); break;
            case NativeInt:           stack.push<ili::NativeInt>(static_cast<Variable<ili::NativeInt>*>(variable.get())->value); break;
            case NativeUnsignedInt:   stack.push<ili::NativeUnsignedInt>(static_cast<Variable<ili::NativeUnsignedInt>*>(variable.get())->value); break;
            case F:                   stack.push<Float>(static_cast<Variable<Float>*>(variable.get())->value); break;
            case O:                   stack.push<ManagedPointer>(static_cast<Variable<ManagedPointer>*>(variable.get())->value); break;
            case Pointer:             stack.push<UnmanagedPointer>(static_cast<Variable<UnmanagedPointer>*>(variable.get())->value); break;
            case Invalid:             throw std::runtime_error("Invalid method token type");
        }
    }

    std::unique_ptr<VariableBase> Runtime::createHeapObject(std::size_t size) {
        const auto heapKey = m_heapKey;
        auto [it, inserted] = m_heap.emplace(heapKey, std::vector<u8>(size, 0x00));

        if (!inserted) {
            throw std::runtime_error("Heap object already exists");
        }

        m_heapKey += 1;

        return std::make_unique<Variable<ManagedPointer>>(ValueType::O, ManagedPointer(reinterpret_cast<u64>(it->second.data())));
    }


    void Runtime::ldsflda(Method &method, table::Token token) {
        const auto field = loadStaticField(method, token);

        storeVariableOnStack(std::make_unique<Variable<ManagedPointer>>(ValueType::O, ManagedPointer(reinterpret_cast<u64>(m_staticVariables[field].get()))));
    }

    void Runtime::ldsfld(Method &method, table::Token token) {
        const auto field = loadStaticField(method, token);

        storeVariableOnStack(m_staticVariables[field]);
    }

    void Runtime::stsfld(Method &method, table::Token token) {
        const auto field = loadStaticField(method, token);

        m_staticVariables[field] = createVariableFromStackContent();
    }

    void Runtime::pop() {
        switch (m_stack.getTypeOnStack()) {
            using enum ValueType;
            case Int32:             m_stack.pop<i32>();                     break;
            case Int64:             m_stack.pop<i64>();                     break;
            case NativeInt:         m_stack.pop<ili::NativeInt>();          break;
            case NativeUnsignedInt: m_stack.pop<ili::NativeUnsignedInt>();  break;
            case F:                 m_stack.pop<Float>();                   break;
            case O:                 m_stack.pop<ManagedPointer>();          break;
            case Pointer:           m_stack.pop<UnmanagedPointer>();        break;
            case Invalid:           throw std::runtime_error("Invalid method token type");
        }
    }

    void Runtime::newobj(Method& method, table::Token token) {
        switch (token.getId()) {
            case table::MethodDef::ID: {
                const auto &executable = method.getAssembly();
                auto methodToCall = Method(executable, token);

                const auto methodExecutable = methodToCall.getAssembly();
                const auto methodDef = methodToCall.getMethodDef();
                const auto typeDef = executable->getTypeDefOfMethod(methodDef);

                const auto objectSize = methodExecutable->getTypeSize(typeDef);
                const auto newObjectReference = createHeapObject(objectSize);

                storeVariableOnStack(newObjectReference);

                executeInstructions(methodToCall);
                break;
            }
            case table::MemberRef::ID: {
                const auto qualifiedMethodName = method.getAssembly()->getQualifiedMemberName(token);
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

                const auto typeDef = assembly.getTypeDefOfMethod(methodDef);
                const auto objectSize = assembly.getTypeSize(typeDef);
                const auto newObjectReference = createHeapObject(objectSize);

                storeVariableOnStack(newObjectReference);

                Method methodToCall(&assembly, assembly.getTokenOfTableEntry(methodDef));
                executeInstructions(methodToCall);

                break;
            }
            default: throw std::runtime_error("Invalid call token type");
        }
    }

    void Runtime::ldloca(Method &method, u16 id) {
        m_stack.push(UnmanagedPointer(reinterpret_cast<u64>(method.getLocalVariable(id).get())));
    }

    void Runtime::executeInstructions(Method& method) {
        for (const auto instruction : method.getInstructions()) {
            fmt::println("{}", magic_enum::enum_name(instruction.getOpcode()));

            switch (instruction.getOpcode()) {
                using enum op::Opcode;
                case Nop:       nop();                                                            break;
                case Brk:       brk();                                                            break;
                case Call:      call(method, instruction.get<table::Token>(0));    break;
                case Ldstr:     ldstr(method, instruction.get<table::Token>(0));    break;
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
                case Ldsflda:   ldsflda(method, instruction.get<table::Token>(0)); break;
                case Ldsfld:    ldsfld(method, instruction.get<table::Token>(0));  break;
                case Ldc_i4:    ldc<i32>(instruction.get<i32>(0));                     break;
                case Ldc_i4_s:  ldc<i32>(instruction.get<i8>(0));                      break;
                case Ldc_i8:    ldc<i64>(instruction.get<i64>(0));                     break;
                case Ldc_r4:    ldc<Float>(instruction.get<float>(0));                 break;
                case Ldc_r8:    ldc<Float>(instruction.get<double>(0));                break;
                case Ldc_i4_0:  ldc<i32>(0);                                                 break;
                case Ldc_i4_1:  ldc<i32>(1);                                                 break;
                case Ldc_i4_2:  ldc<i32>(2);                                                 break;
                case Ldc_i4_3:  ldc<i32>(3);                                                 break;
                case Ldc_i4_4:  ldc<i32>(4);                                                 break;
                case Ldc_i4_5:  ldc<i32>(5);                                                 break;
                case Ldc_i4_6:  ldc<i32>(6);                                                 break;
                case Ldc_i4_7:  ldc<i32>(7);                                                 break;
                case Ldc_i4_8:  ldc<i32>(8);                                                 break;
                case Ldc_i4_m1: ldc<i32>(-1);                                                break;
                case Stsfld:    stsfld(method, instruction.get<table::Token>(0));  break;
                case Pop:       pop();                                                            break;
                case Newobj:    newobj(method, instruction.get<table::Token>(0));  break;

                default:
                    throw std::runtime_error(fmt::format("Unimplemented opcode {} (0x{:02})", magic_enum::enum_name(instruction.getOpcode()), u8(instruction.getOpcode())));
            }
        }
    }

}