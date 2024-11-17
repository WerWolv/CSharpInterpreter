#pragma once

#include <list>
#include <vector>
#include <functional>
#include <set>
#include <utility>

#include <ili/assembly.hpp>
#include <ili/tables.hpp>
#include <ili/opcodes.hpp>
#include <ili/utils.hpp>

#include <fmt/format.h>

namespace ili {

    enum class ValueType : u8 {
        Invalid                 = 0,
        Int32                   = 1,
        Int64                   = 2,
        NativeInt               = 4,
        NativeUnsignedInt       = 8,
        F                       = 16,
        O                       = 32,
        Pointer                 = 64
    };

    struct VariableBase {
        explicit VariableBase(ValueType type) : type(type) {}

        ValueType type;
    };

    template<typename T>
    struct Variable : VariableBase {
        Variable(ValueType type, T value) : VariableBase(type), value(value) {}
        T value;
    };

    constexpr u8 getTypeSize(ValueType type) {
        switch (type) {
            using enum ValueType;

            case Int32: return 4;
            case Int64: return 8;
            case NativeInt: return 8;
            case NativeUnsignedInt: return 8;
            case F: return 8;
            case O: return 8;
            case Pointer: return 8;
            default: return 0;
        }
    }

    class Stack {
    public:
        Stack() = default;
        explicit Stack(std::size_t size) : m_stack(size), m_stackPointer(m_stack.data()) {}
        Stack(const Stack&) = delete;
        Stack(Stack &&other) noexcept {
            m_stack = std::move(other.m_stack);
            m_stackPointer = other.m_stackPointer;
            m_typeStack = std::move(other.m_typeStack);
        }

        Stack& operator=(const Stack &other) = delete;
        Stack& operator=(Stack &&other) noexcept {
            m_stack = std::move(other.m_stack);
            m_stackPointer = other.m_stackPointer;
            m_typeStack = std::move(other.m_typeStack);

            return *this;
        }

        ~Stack() = default;

        [[nodiscard]] ValueType getTypeOnStack(u16 pos = 0) const {
            return m_typeStack[m_typeStack.size() - 1 - pos];
        }

        template<typename T>
        T pop() {
            auto ret = T();

            size_t sizeToPop = getTypeSize(getTypeOnStack());

            if (sizeToPop > sizeof(T)) {
                throw std::out_of_range("Not enough data on stack to pop");
            }

            if (getTypeOnStack() != getValueType<T>()) {
                throw std::runtime_error("Tried to pop different type than was on stack");
            }

            m_typeStack.pop_back();
            m_stackPointer -= sizeof(T);

            if (m_stackPointer < m_stack.data()) {
                throw std::out_of_range("Stack underflow");
            }

            std::memset(&ret, 0x00, sizeof(T));
            std::memcpy(&ret, m_stackPointer, sizeToPop);

            return ret;
        }

        template<typename T>
        static constexpr ValueType getValueType() {
            if constexpr (std::same_as<T, i32>) return ValueType::Int32;
            if constexpr (std::same_as<T, i64>) return ValueType::Int64;
            if constexpr (std::same_as<T, Float>) return ValueType::F;
            if constexpr (std::same_as<T, NativeInt>) return ValueType::NativeInt;
            if constexpr (std::same_as<T, NativeUnsignedInt>) return ValueType::NativeUnsignedInt;
            if constexpr (std::same_as<T, ManagedPointer>) return ValueType::O;
            if constexpr (std::same_as<T, UnmanagedPointer>) return ValueType::Pointer;

            std::unreachable();
        }

        template<typename T>
        void push(T value) {
            constexpr static auto ValueType = getValueType<T>();
            constexpr static auto ValueSize = getTypeSize(ValueType);
            std::memcpy(m_stackPointer, &value, ValueSize);
            m_typeStack.push_back(ValueType);
            m_stackPointer += sizeof(T);
        }

        [[nodiscard]] std::ptrdiff_t getUsedStackSize() const {
            return m_stackPointer - m_stack.data();
        }

    private:
        std::vector<u8> m_stack;
        std::vector<ValueType> m_typeStack;
        u8 *m_stackPointer = nullptr;
    };

    class Method {
    public:
        Method(const Assembly *assembly, table::Token methodToken);

        [[nodiscard]] const table::MethodDef* getMethodDef() const;
        [[nodiscard]] table::Token getToken() const;
        [[nodiscard]] std::span<const u8> getByteCode() const;

        [[nodiscard]] util::Generator<op::Instruction> getInstructions();
        [[nodiscard]] const Assembly* getAssembly() const { return m_assembly; }

        [[nodiscard]] auto& getLocalVariable(u16 index) {
            return m_localVariables[index];
        }

        [[nodiscard]] const auto& getLocalVariable(u8 index) const {
            return m_localVariables[index];
        }

        void offsetProgramCounter(i64 programCounter) {
            m_instructionOffset = u64(i64(m_instructionOffset) + programCounter);
        }

    private:
        const Assembly *m_assembly;

        table::Token m_methodToken;
        mutable const table::MethodDef *m_methodDef = nullptr;
        mutable std::span<const u8> m_byteCode;
        u64 m_instructionOffset = 0x00;

        std::array<std::unique_ptr<VariableBase>, 0xFF> m_localVariables;
    };

    class Runtime {
    public:
        Runtime() = default;

        i32 run(Assembly &&assembly);

        using AssemblyLoaderFunction = std::function<std::optional<Assembly>(const std::string &assemblyName)>;
        void addAssemblyLoader(const AssemblyLoaderFunction &function);
        void addAssembly(Assembly &&assembly);

    private:
        void nop();
        void brk();
        void call(Method &method, table::Token token);
        void ldstr(Method &method, table::Token value);
        void ldloca(Method &method, u16 id);
        void ldarg(Method &method, u16 id);
        void stloc(Method &method, u16 id);
        void ldloc(Method& method, u16 id);
        void br(Method &method, i32 offset);
        void ldsflda(Method &method, table::Token token);
        void ldsfld(Method &method, table::Token token);
        void stsfld(Method &method, table::Token token);
        void pop();
        void newobj(Method &method, table::Token token);

        template<typename T>
        void ldc(auto value) {
            m_stack.push<T>(value);
        }

    private:
        void executeInstructions(Method &method);
        const table::Field* loadStaticField(Method &method, table::Token token);

        std::unique_ptr<VariableBase> createVariableFromStackContent();
        void storeVariableOnStack(const std::unique_ptr<VariableBase> &variable);
        std::unique_ptr<VariableBase> createHeapObject(std::size_t size);

    private:
        std::map<std::string, Assembly> m_assemblies;
        std::list<Method> m_methodStack;
        std::list<AssemblyLoaderFunction> m_assemblyLoaders;
        Stack m_stack;
        std::set<const table::TypeDef*> m_initializedTypes;

        std::map<const table::Field*, std::unique_ptr<VariableBase>> m_staticVariables;
        u64 m_heapKey = 0;
        std::map<u64, std::vector<u8>> m_heap;
    };

}
