#pragma once

// ---- CPP TRAITS LIBRARY ----
// Author: Lucas Costa Ferreira
//
// Read this header in order:
//   1. C++ helpers for views, ownership, signatures, and merge access.
//   2. Macro helpers for joining tokens and processing lists.
//   3. Method descriptors: the stored tokens from method(...) and param(...).
//   4. Macros that turn those descriptors into declarations, checks, and calls.
//   5. Trait generators and public keyword macros.
//
// A method is described once. The generator reuses that description to declare
// its interface, check implementations, and generate adapter methods. Expansion
// happens during preprocessing; the generated methods run as ordinary C++ code.
//
// Trait<T> accepts a valid impl mapping or a compatible interface implementation.
// A mapping does not add methods or bases to T; use an adapter for runtime calls.
//
// Helpers appear before the macros that use them. Inside continued macros, use
// block comments and keep each trailing backslash; // would hide the next lines.

#include <concepts>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace cpptraits_detail {
    // ---- Runtime composition: store one pointer for each trait interface. ----
    // Later, as<Interface>() selects a pointer with std::get<Interface*>.
    // That lookup requires unique types. Compare each type against the remaining
    // types to reject repeated interfaces before building the pointer tuple.
    template<typename... Types>
    struct unique_types : std::true_type {};

    template<typename First, typename... Rest>
    struct unique_types<First, Rest...>
        : std::bool_constant<(!(std::same_as<First, Rest> || ...)) &&
                             unique_types<Rest...>::value> {};

    template<typename T, typename... Interfaces>
    class owned_bundle;

    // A view borrows existing interfaces from one object. ReadOnly selects
    // const Interface* instead of Interface*; it does not change object ownership.
    // The interface marker rejects nested bundles as composition components.
    // Public construction binds the pointers immediately. owned_bundle uses the
    // protected default constructor, then binds after its stored object exists.
    template<bool ReadOnly, typename... Interfaces>
    class interface_view {
        static_assert(sizeof...(Interfaces) > 0, "A composed trait needs components");
        static_assert(unique_types<Interfaces...>::value,
                      "A composed trait cannot repeat an interface");
        static_assert(((requires { typename Interfaces::cpptraits_ordinary_interface; }) && ...),
                      "Components must be ordinary runtime trait interfaces");

        // Preserve the source's constness in each stored interface pointer.
        template<typename Interface>
        using selected_type = std::conditional_t<ReadOnly, const Interface, Interface>;

        std::tuple<selected_type<Interfaces>*...> interfaces_{};

        template<typename T, typename... OtherInterfaces>
        friend class owned_bundle;

    protected:
        interface_view() = default;

        // Owned bundles inherit interface_view. Assignment through that base copies
        // incoming interface pointers but does not assign the owner's stored object.
        // Owners override this hook to rebind those pointers to their own object_,
        // preserving the invariant that an owner's view always refers to its own value.
        // Plain borrowed views leave the copied bindings unchanged.
        virtual void restore_owned_binding() noexcept {}

        // Store pointers to public, unambiguous interface bases already in object.
        // static_cast adjusts their addresses for multiple inheritance.
        // An impl mapping alone provides no base to point to; this does not create
        // adapters or remove constness.
        template<typename T>
            requires (std::convertible_to<T*, selected_type<Interfaces>*> && ...)
        void bind(T& object) noexcept {
            interfaces_ = std::tuple<selected_type<Interfaces>*...>{
                static_cast<selected_type<Interfaces>*>(std::addressof(object))...};
        }

    public:
        // Borrow the object's interfaces without copying the object.
        template<typename T>
            requires (std::convertible_to<T*, selected_type<Interfaces>*> && ...)
        explicit interface_view(T& object) noexcept {
            bind(object);
        }

        // Reject rvalue sources to avoid borrowing a temporary that will expire.
        template<typename T> requires (!std::is_lvalue_reference_v<T>)
        interface_view(T&&) = delete;

        // Plain copies/moves copy only pointer bindings, not the source value.
        // Slicing an owner to a view likewise creates a borrow of the owner.
        interface_view(const interface_view&) = default;
        interface_view(interface_view&&) = default;

        // Rebind borrowed views; owners restore bindings to their own value.
        interface_view& operator=(const interface_view& other) noexcept {
            interfaces_ = other.interfaces_;
            restore_owned_binding();
            return *this;
        }

        interface_view& operator=(interface_view&& other) noexcept {
            interfaces_ = other.interfaces_;
            restore_owned_binding();
            return *this;
        }

        // box(Composition, ...) deletes an owned_bundle through this base.
        virtual ~interface_view() = default;

        // Return the selected interface by reference; no RTTI lookup is needed.
        // Either a const source or a const view makes the reference const.
        // Method calls through it still use normal virtual dispatch.
        template<typename Interface>
            requires (std::same_as<Interface, Interfaces> || ...)
        selected_type<Interface>& as() & noexcept {
            return *std::get<selected_type<Interface>*>(interfaces_);
        }

        template<typename Interface>
            requires (std::same_as<Interface, Interfaces> || ...)
        const Interface& as() const & noexcept {
            return *std::get<selected_type<Interface>*>(interfaces_);
        }

        // Forbid extracting references from a temporary.
        template<typename Interface>
        void as() && = delete;
        template<typename Interface>
        void as() const && = delete;
    };

    // ---- Owning composition: store one T and a view of its interfaces. ----
    // The view's pointers must always refer to this object's object_ member.
    // T is stored inline, not allocated separately. box allocates the whole bundle.
    // The base view permits mutable access; const owners use its const overloads.
    template<typename T, typename... Interfaces>
    class owned_bundle final : public interface_view<false, Interfaces...> {
        using base_type = interface_view<false, Interfaces...>;
        T object_;

        void restore_owned_binding() noexcept override {
            this->bind(object_);
        }

    public:
        // Bind only after the member exists: the base is initialized before T.
        explicit owned_bundle(T object) : object_(std::move(object)) {
            this->bind(object_);
        }

        // Copy the value, not the source's pointers. Bind new pointers to this copy.
        owned_bundle(const owned_bundle& other) requires std::copy_constructible<T>
            : base_type(), object_(other.object_) {
            this->bind(object_);
        }

        // Moving also creates a new member at a new address, so bind it again.
        owned_bundle(owned_bundle&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
            : base_type(), object_(std::move(other.object_)) {
            this->bind(object_);
        }

        // Assign only the value. object_ stays at the same address, so its existing
        // interface bindings remain valid and the view base need not be assigned.
        owned_bundle& operator=(const owned_bundle& other)
            requires std::is_copy_assignable_v<T> {
            if (this != std::addressof(other)) {
                object_ = other.object_;
            }
            return *this;
        }

        owned_bundle& operator=(owned_bundle&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
            requires std::is_move_assignable_v<T> {
            if (this != std::addressof(other)) {
                object_ = std::move(other.object_);
            }
            return *this;
        }
    };

    // ---- Mapping signatures: make the member's receiver an explicit argument. ----
    // double (I::*)(int) const noexcept becomes double (*)(const T&, int) noexcept.
    // This computes a pointer TYPE for checking static mapping functions; it does
    // not convert or call a function. Preserve parameters, return type, and noexcept.
    // Ref-qualified and volatile members are not handled here. Normal noexcept
    // pointer conversions allow a nonthrowing mapping for a throwing contract.
    template<typename Member, typename T>
    struct implementation_signature;

    template<typename R, typename C, typename... Args, typename T>
    struct implementation_signature<R (C::*)(Args...), T> {
        using type = R (*)(T&, Args...);
    };

    template<typename R, typename C, typename... Args, typename T>
    struct implementation_signature<R (C::*)(Args...) const, T> {
        using type = R (*)(const T&, Args...);
    };

    template<typename R, typename C, typename... Args, typename T>
    struct implementation_signature<R (C::*)(Args...) noexcept, T> {
        using type = R (*)(T&, Args...) noexcept;
    };

    template<typename R, typename C, typename... Args, typename T>
    struct implementation_signature<R (C::*)(Args...) const noexcept, T> {
        using type = R (*)(const T&, Args...) noexcept;
    };

    template<class Pointer>
    struct mapped_function;

    template<class R, class Receiver, class... Args, bool Noexcept>
    struct mapped_function<R (*)(Receiver, Args...) noexcept(Noexcept)> {
        using receiver = Receiver;
        static constexpr bool is_noexcept = Noexcept;
    };

    // Merge methods receive an interface reference but need the wrapped 'const T&'.
    // 'Interface' is the generated runtime interface; 'T' is the type named by impl.
    // Owning and borrowing adapters for that 'Interface' and 'T' share this base, so
    // merge can dynamic_cast its operand to concrete_access<Interface, T> without
    // knowing how it is stored. A different mapped 'T' does not match that base.
    // Ordinary and clone-only traits do not need this helper.
    template<class Interface, class T>
    struct concrete_access : Interface {
        virtual const T& cpptraitsConcreteObject() const noexcept = 0;
    };

    // Adapter is the generated owning or borrowing wrapper inheriting this helper.
    // Passing the derived type as a template argument uses the CRTP pattern. The
    // static_cast recovers that wrapper from this helper's own 'this' pointer;
    // cpptraitsObject() then returns its stored or borrowed value. The wrapper
    // grants friendship so this helper can call that private function.
    // Only merge adapters inherit this helper, where the override is valid.
    template<class Interface, class T, class Adapter>
    struct concrete_adapter : concrete_access<Interface, T> {
        const T& cpptraitsConcreteObject() const noexcept override {
            return static_cast<const Adapter*>(this)->cpptraitsObject();
        }
    };

    // A malformed method descriptor instantiates this template to report its
    // expected argument count. Keeping the assertion in a template means simply
    // that including the header does not trigger it.
    template<bool Valid = false>
    struct invalid_method_arity {
        static_assert(Valid, "method(...) expects 3 or 4 arguments");
        using type = void;
    };
}

// ---- Preprocessor front end: macros to generate the machinery. ----
// CAT has CAT_I inside it because arguments may themselves be macros.
// CAT expands them first, then CAT_I concatenates the resulting tokens using ##.
#define CPPTRAITS_CAT_I(A, B) A##B
#define CPPTRAITS_CAT(A, B) CPPTRAITS_CAT_I(A, B)

// A macro sees the commas between tokens, it does not interpret as
// C++ template argument boundaries.
// Writing (std::pair<int, int>) protects that whole spelling as one macro argument.
// TYPE must accept both this spelling and a simple unwrapped type such as float.
//
// PAREN_PROBE is function-like: a leading '(' invokes it and produces '~, 1'.
// CHECK appends a fallback 0 and selects its second argument, yielding 1 after a
// successful probe or 0 when PAREN_PROBE is not invoked. TYPE_1 then removes ONE
// protective layer of parenthesis; TYPE_0 leaves the tokens alone. Thus:
//   TYPE(float)                 -> float
//   TYPE((std::pair<int, int>)) -> std::pair<int, int>
// This only normalizes the spelling; it does not parse arbitrary declarators.
#define CPPTRAITS_UNPAREN(...) __VA_ARGS__
#define CPPTRAITS_PAREN_PROBE(...) ~, 1
#define CPPTRAITS_CHECK_I(_0, _1, ...) _1
#define CPPTRAITS_CHECK(...) CPPTRAITS_CHECK_I(__VA_ARGS__, 0)
#define CPPTRAITS_IS_PAREN(Type) CPPTRAITS_CHECK(CPPTRAITS_PAREN_PROBE Type)
#define CPPTRAITS_TYPE_0(Type) Type
#define CPPTRAITS_TYPE_1(Type) CPPTRAITS_UNPAREN Type
#define CPPTRAITS_TYPE(Type) \
    CPPTRAITS_CAT(CPPTRAITS_TYPE_, CPPTRAITS_IS_PAREN(Type))(Type)

// ---- Apply a macro to a list of 1-8 items. ----
// EACH(M, Context, a, b) produces M(Context, a) M(Context, b).
// The argument count selects an EACH_N helper, which processes one item and
// passes the rest to EACH_N-1. M supplies any needed commas or other separators.
// Parameter lists use separate PARAMS_N helpers so the two loops can be nested.
#define CPPTRAITS_EACH_1(M, C, A) M(C, A)
#define CPPTRAITS_EACH_2(M, C, A, ...) M(C, A) CPPTRAITS_EACH_1(M, C, __VA_ARGS__)
#define CPPTRAITS_EACH_3(M, C, A, ...) M(C, A) CPPTRAITS_EACH_2(M, C, __VA_ARGS__)
#define CPPTRAITS_EACH_4(M, C, A, ...) M(C, A) CPPTRAITS_EACH_3(M, C, __VA_ARGS__)
#define CPPTRAITS_EACH_5(M, C, A, ...) M(C, A) CPPTRAITS_EACH_4(M, C, __VA_ARGS__)
#define CPPTRAITS_EACH_6(M, C, A, ...) M(C, A) CPPTRAITS_EACH_5(M, C, __VA_ARGS__)
#define CPPTRAITS_EACH_7(M, C, A, ...) M(C, A) CPPTRAITS_EACH_6(M, C, __VA_ARGS__)
#define CPPTRAITS_EACH_8(M, C, A, ...) M(C, A) CPPTRAITS_EACH_7(M, C, __VA_ARGS__)
#define CPPTRAITS_SELECT_EACH(_1, _2, _3, _4, _5, _6, _7, _8, Selected, ...) Selected
#define CPPTRAITS_EACH(M, C, ...)                                                  \
    CPPTRAITS_SELECT_EACH(__VA_ARGS__, CPPTRAITS_EACH_8, CPPTRAITS_EACH_7,           \
        CPPTRAITS_EACH_6, CPPTRAITS_EACH_5, CPPTRAITS_EACH_4, CPPTRAITS_EACH_3,       \
        CPPTRAITS_EACH_2, CPPTRAITS_EACH_1)(M, C, __VA_ARGS__)

// param(Type, name) is a token tuple reused for two independent outputs:
//   param(std::unique_ptr<int>, item)
//      -> declaration: std::unique_ptr<int> item
//      -> argument:    std::forward<decltype(item)>(item)
// Forward parameters according to their declared types: value and rvalue-reference
// parameters may be moved into the mapping; lvalue references remain lvalues.
#define param(Type, Name) (Type, Name)
#define CPPTRAITS_PARAMETER_DECL(Type, Name) CPPTRAITS_TYPE(Type) Name
#define CPPTRAITS_PARAMETER_FORWARD(Type, Name) ::std::forward<decltype(Name)>(Name)

// Process up to eight param(...) tuples and separate their outputs with commas.
// M A uses tuple A's parentheses to call M(Type, name). The same list produces
// declarations or forwarded arguments, depending on M.
// __VA_OPT__ skips processing entirely when there are no parameters.
#define CPPTRAITS_PARAMS_1(M, A) M A
#define CPPTRAITS_PARAMS_2(M, A, ...) M A, CPPTRAITS_PARAMS_1(M, __VA_ARGS__)
#define CPPTRAITS_PARAMS_3(M, A, ...) M A, CPPTRAITS_PARAMS_2(M, __VA_ARGS__)
#define CPPTRAITS_PARAMS_4(M, A, ...) M A, CPPTRAITS_PARAMS_3(M, __VA_ARGS__)
#define CPPTRAITS_PARAMS_5(M, A, ...) M A, CPPTRAITS_PARAMS_4(M, __VA_ARGS__)
#define CPPTRAITS_PARAMS_6(M, A, ...) M A, CPPTRAITS_PARAMS_5(M, __VA_ARGS__)
#define CPPTRAITS_PARAMS_7(M, A, ...) M A, CPPTRAITS_PARAMS_6(M, __VA_ARGS__)
#define CPPTRAITS_PARAMS_8(M, A, ...) M A, CPPTRAITS_PARAMS_7(M, __VA_ARGS__)
#define CPPTRAITS_PARAM_MAP(M, ...)                                                \
    CPPTRAITS_SELECT_EACH(__VA_ARGS__, CPPTRAITS_PARAMS_8, CPPTRAITS_PARAMS_7,       \
        CPPTRAITS_PARAMS_6, CPPTRAITS_PARAMS_5, CPPTRAITS_PARAMS_4, CPPTRAITS_PARAMS_3, \
        CPPTRAITS_PARAMS_2, CPPTRAITS_PARAMS_1)(M, __VA_ARGS__)
#define CPPTRAITS_PARAMETER_DECLARATIONS(...) \
    __VA_OPT__(CPPTRAITS_PARAM_MAP(CPPTRAITS_PARAMETER_DECL, __VA_ARGS__))
#define CPPTRAITS_PARAMETER_ARGUMENTS(...) \
    __VA_OPT__(CPPTRAITS_PARAM_MAP(CPPTRAITS_PARAMETER_FORWARD, __VA_ARGS__))

// Give every method the same seven-field descriptor so later macros can process
// parameterized and parameterless methods uniformly.
// The short form supplies Parameters=() for the declaration and Arguments=().
// Required methods also have an empty Body field because they provide no body.
// The full form builds declarations and forwarding arguments from the param list:
//   method(void, set, (param(int, value)), )
// becomes:
//   (REQUIRED, void, set, (int value),
//       (std::forward<decltype(value)>(value)), <empty qualifiers>, ())
//
// Parentheses keep the descriptor and its comma-containing fields together
// while other macros iterate over the methods.
#define CPPTRAITS_METHOD_3(Return, Name, Qualifiers) \
    (REQUIRED, Return, Name, (), (), Qualifiers, ())
#define CPPTRAITS_METHOD_4(Return, Name, Parameters, Qualifiers)                    \
    (REQUIRED, Return, Name, (CPPTRAITS_PARAMETER_DECLARATIONS Parameters),        \
        (CPPTRAITS_PARAMETER_ARGUMENTS Parameters), Qualifiers, ())
#define CPPTRAITS_METHOD_INVALID(...) \
    (REQUIRED, typename ::cpptraits_detail::invalid_method_arity<>::type, cpptraits_invalid_method, (), (), , ())

// A default uses the same descriptor but retains its user-provided braced body.
// Body is variadic because braces do not protect commas from the
// preprocessor (e.g. return {first, second};). Rejoining __VA_ARGS__ inside a
// protective (...) preserves those commas; DECLARE_DEFAULT removes that wrapper
// when inserting the body. The braces themselves remain as ordinary C++ syntax.
#define default_method(Return, Name, Parameters, Qualifiers, ...)                 \
    (DEFAULT, Return, Name, (CPPTRAITS_PARAMETER_DECLARATIONS Parameters),         \
        (CPPTRAITS_PARAMETER_ARGUMENTS Parameters), Qualifiers, (__VA_ARGS__))

// Appending a supported list of candidate names makes Selected land on METHOD_3 or
// METHOD_4 for the supported arities. Other arities within the bounded selector
// choose METHOD_INVALID, which emits a descriptor containing invalid_method_arity
// so the eventual C++ diagnostic explains the syntax error neatly.
#define CPPTRAITS_SELECT_METHOD(_1, _2, _3, _4, _5, Selected, ...) Selected
#define method(...)                                                               \
    CPPTRAITS_SELECT_METHOD(__VA_ARGS__, CPPTRAITS_METHOD_INVALID,                  \
        CPPTRAITS_METHOD_4, CPPTRAITS_METHOD_3, CPPTRAITS_METHOD_INVALID,           \
        CPPTRAITS_METHOD_INVALID)(__VA_ARGS__)

// ---- Concrete method declaration ----
// concrete_method(name, shape) dispatches directly on the literal shape token.
// Both shapes still fit the same seven-field tuple and all ordinary passes:
//   merge -> void name(const Interface&)
//   clone -> unique_ptr<Interface> name() const
// Only their REQUIRE_IMPL/FORWARD consumers substitute the mapping's concrete T.
#define CPPTRAITS_CONCRETE_merge(Name)                                            \
    (MERGE, void, Name, (const cpptraits_interface_type& cpptraitsOther),           \
        (cpptraitsOther), , ())
#define CPPTRAITS_CONCRETE_clone(Name)                                            \
    (CLONE, ::std::unique_ptr<cpptraits_interface_type>, Name, (), (), const, ())
#define concrete_method(Name, Shape) CPPTRAITS_CAT(CPPTRAITS_CONCRETE_, Shape)(Name)

// Each method contributes '|| true' or '|| false' after an initial false.
// The resulting boolean tells the generator whether any concrete or merge method
// exists. Both shapes require impl mappings; only merge needs the RTTI access base.
#define CPPTRAITS_CONCRETE_FLAG_REQUIRED || false
#define CPPTRAITS_CONCRETE_FLAG_DEFAULT || false
#define CPPTRAITS_CONCRETE_FLAG_MERGE || true
#define CPPTRAITS_CONCRETE_FLAG_CLONE || true
#define CPPTRAITS_CONCRETE_FLAG(Kind, ...) CPPTRAITS_CAT(CPPTRAITS_CONCRETE_FLAG_, Kind)
#define CPPTRAITS_MERGE_FLAG_REQUIRED || false
#define CPPTRAITS_MERGE_FLAG_DEFAULT || false
#define CPPTRAITS_MERGE_FLAG_MERGE || true
#define CPPTRAITS_MERGE_FLAG_CLONE || false
#define CPPTRAITS_MERGE_FLAG(Kind, ...) CPPTRAITS_CAT(CPPTRAITS_MERGE_FLAG_, Kind)

// Apply a chosen macro to every method descriptor.
// APPLY_METHOD uses the descriptor's parentheses to form the macro call:
//   APPLY_METHOD(DECLARE_METHOD, (REQUIRED, ...))
//   becomes DECLARE_METHOD(REQUIRED, ...).
// METHODS repeats this for the whole list, letting us reuse it to generate
// declarations, signature checks, and forwarding methods.
#define CPPTRAITS_APPLY_METHOD(Generator, Descriptor) Generator Descriptor
#define CPPTRAITS_METHODS(Generator, ...) \
    CPPTRAITS_EACH(CPPTRAITS_APPLY_METHOD, Generator, __VA_ARGS__)

// An associated type has the same descriptor layout as a method. Markers expand
// to 1, so the first token is 1 when present and the trailing 0 otherwise.
#define associated_type(Name) (ASSOCIATED, , Name, , , , )
#define CPPTRAITS_ASSOCIATED_MARKER_REQUIRED
#define CPPTRAITS_ASSOCIATED_MARKER_DEFAULT
#define CPPTRAITS_ASSOCIATED_MARKER_MERGE
#define CPPTRAITS_ASSOCIATED_MARKER_CLONE
#define CPPTRAITS_ASSOCIATED_MARKER_ASSOCIATED 1,
#define CPPTRAITS_ASSOCIATED_MARKER(Kind, ...) CPPTRAITS_ASSOCIATED_MARKER_##Kind
#define CPPTRAITS_FIRST(First, ...) First
#define CPPTRAITS_HAS_ASSOCIATED_I(...) CPPTRAITS_FIRST(__VA_ARGS__ 0)
#define CPPTRAITS_HAS_ASSOCIATED(...) \
    CPPTRAITS_HAS_ASSOCIATED_I(CPPTRAITS_METHODS(CPPTRAITS_ASSOCIATED_MARKER, __VA_ARGS__))

// ---- Descriptor consumers ----
// Every macro receives the same seven fields:
//   (Kind, Return, Method, Parameters, Arguments, Qualifiers, Body)
// Parameters includes its own (...); Arguments holds forwarded expressions in
// (...); Body wraps the user's braced body in one additional protective (...).
// Consumers intentionally ignore fields they do not need. Kind dispatch selects
// REQUIRED, DEFAULT, MERGE or CLONE.
//
// Declaration pass:
// - ordinary required members are pure virtual, defaults carry
// their provided body.
// - Merge/clone also declare pure virtual members, but their
// runtime signatures come from their fixed-shape descriptors, not the mapping's
// concrete signature.
// The injected interface class name is in scope here, which
// permits its own reference and unique_ptr types to appear in those signatures.
#define CPPTRAITS_DECLARE_REQUIRED(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    virtual CPPTRAITS_TYPE(Return) Method Parameters Qualifiers = 0;
#define CPPTRAITS_DECLARE_DEFAULT(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    virtual CPPTRAITS_TYPE(Return) Method Parameters Qualifiers CPPTRAITS_UNPAREN Body
#define CPPTRAITS_DECLARE_MERGE(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    virtual void Method Parameters = 0;
#define CPPTRAITS_DECLARE_CLONE(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    virtual CPPTRAITS_TYPE(Return) Method() const = 0;
#define CPPTRAITS_DECLARE_METHOD(Kind, ...) \
    CPPTRAITS_CAT(CPPTRAITS_DECLARE_, Kind)(__VA_ARGS__)

// Check that the type has a method with the required signature, selecting the
// matching overload if needed. Checking the member-function pointer prevents
// accepting a different signature just because a call could convert its arguments.
//
// Interface inheritance is checked separately. Abstract types can also pass:
// satisfying the runtime contract does not require creating an instance.
#define CPPTRAITS_REQUIRE_METHOD(Kind, Return, Method, Parameters, Arguments, Qualifiers, Body) \
    static_cast<CPPTRAITS_TYPE(Return) (CppTraitsSelf::*) Parameters Qualifiers>(    \
        &CppTraitsSelf::Method);

// Find the mapping's expected function-pointer type from the interface method.
// implementation_signature replaces the implicit receiver with T& or const T&.
// IMPL_POINTER produces that type; MATCHES_IMPL checks for a matching static
// function. Self is the receiver type, including const when borrowing const T.
#define CPPTRAITS_IMPL_POINTER(Self, Interface, Return, Method, Parameters, Qualifiers) \
    typename ::cpptraits_detail::implementation_signature<                         \
        decltype(static_cast<CPPTRAITS_TYPE(Return) (Interface::*) Parameters Qualifiers>( \
            &Interface::Method)), Self>::type
#define CPPTRAITS_MATCHES_IMPL(Impl, Self, Interface, Return, Method, Parameters, Qualifiers) \
    requires {                                                                    \
        static_cast<CPPTRAITS_IMPL_POINTER(Self, Interface, Return, Method, Parameters, Qualifiers)>( \
            &Impl::Method);                                                       \
    }

// For a default method such as isStopped, there are three cases:
//   - No isStopped member in impl: use the trait's default.
//   - A matching isStopped function: use that implementation.
//   - An invalid isStopped function or data member: reject the mapping.
// This helper checks whether the name exists; signature checking is separate.
//
// The probe inherits the mapping and a fallback with a same-named member.
// Finding only the fallback means 'absent' (false); ambiguous lookup through
// both bases means 'present' (true), even for private members or overload sets.
// Incomplete/final mappings cannot use this probe, so return true conservatively
// and require a matching function. Final mappings must provide default overrides;
// other unusual non-derivable mappings remain unsupported.
#define CPPTRAITS_HAS_IMPL_NAME(Impl, Method)                                      \
    ([]<typename CppTraitsMapping>() constexpr {                                  \
        if constexpr (!requires { sizeof(CppTraitsMapping); }) {                   \
            return true;                                                          \
        } else if constexpr (::std::is_final_v<CppTraitsMapping>) {                \
            return true;                                                          \
        } else {                                                                  \
            struct CppTraitsNameFallback { int Method; };                          \
            struct CppTraitsNameProbe : CppTraitsMapping, CppTraitsNameFallback {}; \
            return !requires { &CppTraitsNameProbe::Method; };                     \
        }                                                                         \
    }.template operator()<Impl>())

// Required methods need a matching static function. Defaults may be omitted,
// but a supplied member must match. These checks go inside requires { ... }.
// The extra 'requires' on the default check demands a true result, not merely
// a boolean expression that compiles.
#define CPPTRAITS_REQUIRE_IMPL_REQUIRED(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    static_cast<CPPTRAITS_IMPL_POINTER(CppTraitsSelf, CppTraitsInterface, Return, Method, Parameters, Qualifiers)>( \
        &CppTraitsImpl::Method);
#define CPPTRAITS_REQUIRE_IMPL_DEFAULT(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    requires (CPPTRAITS_MATCHES_IMPL(CppTraitsImpl, CppTraitsSelf, CppTraitsInterface, Return, Method, Parameters, Qualifiers) \
        || !CPPTRAITS_HAS_IMPL_NAME(CppTraitsImpl, Method));

// Merge maps to void(T&, const T&), not the interface-reference signature.
// Reject const receivers explicitly: a merge adapter must be able to mutate T.
#define CPPTRAITS_REQUIRE_IMPL_MERGE(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    requires (!::std::is_const_v<CppTraitsSelf>);                                   \
    static_cast<void (*)(CppTraitsSelf&, const CppTraitsSelf&)>(&CppTraitsImpl::Method);
// Clone maps to T(const T&); the adapter supplies the owning interface pointer.
// Remove const from T so a const borrow can produce a new mutable value.
// Require std::move_constructible<T> to place that value in owned storage;
// construction from an rvalue may use either a move or a copy constructor.
#define CPPTRAITS_REQUIRE_IMPL_CLONE(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    requires ::std::move_constructible<::std::remove_const_t<CppTraitsSelf>>;       \
    static_cast<::std::remove_const_t<CppTraitsSelf> (*)(                          \
        const ::std::remove_const_t<CppTraitsSelf>&)>(&CppTraitsImpl::Method);
// Select the validator after defining each supported kind above.
#define CPPTRAITS_REQUIRE_IMPL(Kind, ...) \
    CPPTRAITS_CAT(CPPTRAITS_REQUIRE_IMPL_, Kind)(__VA_ARGS__)

// Add a comma before nonempty arguments: f(object) or f(object, x).
// The caller supplies the receiver and unwraps the Arguments tuple.
#define CPPTRAITS_EXTRA_ARGUMENTS(...) __VA_OPT__(,) __VA_ARGS__

// ---- Adapter methods: forward interface calls to the static mapping. ----
// cpptraitsObject() returns the wrapped object, whether owned or borrowed.
// Required methods pass it as the first argument, followed by the method's
// arguments. 'return' also works when the mapping returns void.
#define CPPTRAITS_FORWARD_REQUIRED(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    CPPTRAITS_TYPE(Return) Method Parameters Qualifiers override {                 \
        return CppTraitsImpl::Method(cpptraitsObject()                              \
            CPPTRAITS_EXTRA_ARGUMENTS Arguments);                                  \
    }
// Call a matching default override if the mapping provides one. Otherwise call
// the interface's default body. if constexpr discards the unused branch.
// Qualifying the fallback call avoids dispatching back into this adapter method;
// calls inside the default still dispatch normally. Keep the receiver's constness
// when selecting the mapping overload.
#define CPPTRAITS_FORWARD_DEFAULT(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    CPPTRAITS_TYPE(Return) Method Parameters Qualifiers override {                 \
        using CppTraitsSelf = ::std::remove_reference_t<decltype(cpptraitsObject())>; \
        using CppTraitsPointer = CPPTRAITS_IMPL_POINTER(CppTraitsSelf,             \
            cpptraits_interface_type, Return, Method, Parameters, Qualifiers);     \
        if constexpr (requires { static_cast<CppTraitsPointer>(&CppTraitsImpl::Method); }) { \
            return static_cast<CppTraitsPointer>(&CppTraitsImpl::Method)(          \
                cpptraitsObject() CPPTRAITS_EXTRA_ARGUMENTS Arguments);            \
        } else {                                                                  \
            return cpptraits_interface_type::Method Arguments;                    \
        }                                                                         \
    }

// Check that the operand wraps the same mapped T, using the access base shared
// by owning and borrowing adapters. If dynamic_cast returns nullptr, throw
// invalid_argument before calling the mapping. Otherwise retrieve const T& and
// call the exact mapping overload. Its exceptions propagate unchanged.
// This is the only method shape that requires RTTI.
#define CPPTRAITS_FORWARD_MERGE(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    void Method Parameters override {                                            \
        using CppTraitsConcrete = ::std::remove_const_t<T>;                       \
        using CppTraitsAccess = ::cpptraits_detail::concrete_access<              \
            cpptraits_interface_type, CppTraitsConcrete>;                         \
        const auto* cpptraitsCompatible =                                        \
            dynamic_cast<const CppTraitsAccess*>(&cpptraitsOther);                \
        if (!cpptraitsCompatible) {                                              \
            throw ::std::invalid_argument(#Method ": incompatible concrete types"); \
        }                                                                         \
        using CppTraitsPointer = void (*)(CppTraitsConcrete&, const CppTraitsConcrete&); \
        static_cast<CppTraitsPointer>(&CppTraitsImpl::Method)(                    \
            cpptraitsObject(), cpptraitsCompatible->cpptraitsConcreteObject());  \
    }

// Call the clone mapping and store its returned T in a new owning adapter.
// Clone the underlying value, not the source wrapper, even when it is borrowed.
// The result exposes the whole same trait and can itself be cloned. No RTTI is
// needed because T is already known. make_unique allocates the adapter and inline
// value together; copying resources inside T is the mapping's responsibility.
// Mapping, construction, and allocation exceptions propagate normally.
#define CPPTRAITS_FORWARD_CLONE(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    CPPTRAITS_TYPE(Return) Method() const override {                              \
        using CppTraitsConcrete = ::std::remove_const_t<T>;                       \
        using CppTraitsPointer = CppTraitsConcrete (*)(const CppTraitsConcrete&);  \
        return ::std::make_unique<cpptraits_owning_adapter<CppTraitsConcrete>>(    \
            static_cast<CppTraitsPointer>(&CppTraitsImpl::Method)(cpptraitsObject())); \
    }
// Select the forwarding body after defining each supported kind above.
#define CPPTRAITS_FORWARD_METHOD(Kind, ...) \
    CPPTRAITS_CAT(CPPTRAITS_FORWARD_, Kind)(__VA_ARGS__)

#define CPPTRAITS_STATIC_SIGNATURE(Class, Return, Parameters, Qualifiers, Self) \
    typename ::cpptraits_detail::implementation_signature< \
        CPPTRAITS_TYPE(Return) (Class::*) Parameters Qualifiers, Self>::type

// Expose the declared overloads and call the exact mapping function selected
// during validation. A const operation can be called on a const receiver even
// when other methods require a mutable receiver.
#define CPPTRAITS_MAPPED_CALL_POINTER(Return, Parameters, Qualifiers) \
    CPPTRAITS_STATIC_SIGNATURE(cpptraits_mapped_calls, Return, Parameters, Qualifiers, CppTraitsSelf)
#define CPPTRAITS_MAPPED_CALL_SIGNATURE(Return, Parameters, Qualifiers) \
    ::cpptraits_detail::mapped_function< \
        CPPTRAITS_MAPPED_CALL_POINTER(Return, Parameters, Qualifiers)>
#define CPPTRAITS_MAPPED_CALL_REQUIRED(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    static CPPTRAITS_TYPE(Return) Method( \
        typename CPPTRAITS_MAPPED_CALL_SIGNATURE(Return, Parameters, Qualifiers)::receiver cpptraitsReceiver \
        CPPTRAITS_EXTRA_ARGUMENTS Parameters) \
        noexcept(CPPTRAITS_MAPPED_CALL_SIGNATURE(Return, Parameters, Qualifiers)::is_noexcept) { \
        return static_cast<CPPTRAITS_MAPPED_CALL_POINTER(Return, Parameters, Qualifiers)>( \
            &CppTraitsImpl::Method)(cpptraitsReceiver CPPTRAITS_EXTRA_ARGUMENTS Arguments); \
    }
#define CPPTRAITS_MAPPED_CALL_ASSOCIATED(...)
#define CPPTRAITS_MAPPED_CALL_DEFAULT(...)
#define CPPTRAITS_MAPPED_CALL_MERGE(...)
#define CPPTRAITS_MAPPED_CALL_CLONE(...)
#define CPPTRAITS_MAPPED_CALL(Kind, ...) CPPTRAITS_MAPPED_CALL_##Kind(__VA_ARGS__)

// Native calls use the same declared receiver and argument signatures as mapped
// calls. Select the exact member overload, retaining virtual dispatch.
#define CPPTRAITS_NATIVE_CALL_SIGNATURE(Return, Parameters, Qualifiers) \
    ::cpptraits_detail::mapped_function< \
        CPPTRAITS_STATIC_SIGNATURE(cpptraits_native_calls, Return, Parameters, Qualifiers, CppTraitsSelf)>
#define CPPTRAITS_NATIVE_CALL_REQUIRED(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    static CPPTRAITS_TYPE(Return) Method( \
        typename CPPTRAITS_NATIVE_CALL_SIGNATURE(Return, Parameters, Qualifiers)::receiver cpptraitsReceiver \
        CPPTRAITS_EXTRA_ARGUMENTS Parameters) \
        noexcept(CPPTRAITS_NATIVE_CALL_SIGNATURE(Return, Parameters, Qualifiers)::is_noexcept) { \
        return (cpptraitsReceiver.*static_cast< \
            CPPTRAITS_TYPE(Return) (CppTraitsSelf::*) Parameters Qualifiers>( \
                &CppTraitsSelf::Method)) Arguments; \
    }
#define CPPTRAITS_NATIVE_CALL_ASSOCIATED(...)
#define CPPTRAITS_NATIVE_CALL_DEFAULT(...)
#define CPPTRAITS_NATIVE_CALL_MERGE(...)
#define CPPTRAITS_NATIVE_CALL_CLONE(...)
#define CPPTRAITS_NATIVE_CALL(Kind, ...) CPPTRAITS_NATIVE_CALL_##Kind(__VA_ARGS__)

// An empty primary keeps unsupported receivers substitution-friendly. Prefer
// native members without instantiating a possibly absent or invalid mapping.
#define CPPTRAITS_CALL_DISPATCH \
    template<class CppTraitsSelf, bool = cpptraits_runtime_compatible<CppTraitsSelf>> \
    struct cpptraits_calls {}; \
    template<class CppTraitsSelf> requires cpptraits_has_implementation<CppTraitsSelf> \
    struct cpptraits_calls<CppTraitsSelf, false> : cpptraits_mapped_calls<CppTraitsSelf> {}; \
    template<class CppTraitsSelf> \
    struct cpptraits_calls<CppTraitsSelf, true> : cpptraits_native_calls<CppTraitsSelf> {};

// ---- Build the trait's concept, runtime interface, and adapters. ----
// trait(Rotatable, ...) puts generated types in Rotatable_cpptraits_detail;
// dyn(Rotatable) selects its interface. The numbered steps below generate:
//   1. Method flags and the interface.
//   2. Base types for native implementations and adapters.
//   3. Checks for types that already expose the interface.
//   4. Checks for explicit mappings; either path satisfies the public concept.
//   5. Owning and borrowing adapters with generated forwarding methods.
//   6. Factories used by adapt, borrow, and box.
//
// Checking/forwarding macros use CppTraitsSelf, CppTraitsInterface, and
// CppTraitsImpl from the scopes generated below.
// Define impl(Trait, T) before first checking or adapting T: a mapping checked
// while incomplete cannot become valid later in the same compilation.
#define CPPTRAITS_TRAIT_0(Name, Scope, ...)                                         \
    namespace Scope {                                                              \
        /* 1: Check method kinds, then declare the runtime interface. */            \
        inline constexpr bool cpptraits_has_concrete =                            \
            false CPPTRAITS_METHODS(CPPTRAITS_CONCRETE_FLAG, __VA_ARGS__);          \
        inline constexpr bool cpptraits_has_merge =                               \
            false CPPTRAITS_METHODS(CPPTRAITS_MERGE_FLAG, __VA_ARGS__);             \
        struct cpptraits_interface_type {                                         \
            using cpptraits_ordinary_interface = void;                            \
            virtual ~cpptraits_interface_type() = default;                        \
            CPPTRAITS_METHODS(CPPTRAITS_DECLARE_METHOD, __VA_ARGS__)                \
        };                                                                        \
        /* 2: implements(Name) is allowed only without concrete methods. */         \
        /* The alias template checks this constraint when implements uses it. */   \
        /* Merge/clone obtain their concrete T from impl instead. */               \
        template<class... CppTraitsBindings>                                      \
            requires (!cpptraits_has_concrete && sizeof...(CppTraitsBindings) == 0) \
        using cpptraits_native_interface = cpptraits_interface_type;              \
        /* Only merge needs the helper that exposes the wrapped const T&. */       \
        template<class T, class Adapter>                                         \
        using cpptraits_adapter_base = ::std::conditional_t<                      \
            cpptraits_has_merge,                                                  \
            ::cpptraits_detail::concrete_adapter<cpptraits_interface_type,         \
                ::std::remove_const_t<T>, Adapter>, cpptraits_interface_type>;             \
        /* 3: Check interface inheritance and member signatures. */                \
        template<class CppTraitsSelf>                                             \
        concept cpptraits_runtime_compatible =                                    \
            ::std::derived_from<CppTraitsSelf, cpptraits_interface_type> &&         \
            requires { CPPTRAITS_METHODS(CPPTRAITS_REQUIRE_METHOD, __VA_ARGS__) };  \
        /* 4: impl defines this otherwise incomplete mapping template. */          \
        template<class T> struct cpptraits_implementation;                        \
        template<class CppTraitsSelf> struct cpptraits_implementation_check {      \
            using CppTraitsInterface = cpptraits_interface_type;                  \
            using CppTraitsImpl = cpptraits_implementation<                       \
                ::std::remove_const_t<CppTraitsSelf>>;                            \
            static constexpr bool value = requires {                             \
                /* sizeof checks that impl is defined; no object is created. */   \
                sizeof(CppTraitsImpl);                                            \
                CPPTRAITS_METHODS(CPPTRAITS_REQUIRE_IMPL, __VA_ARGS__)             \
            };                                                                    \
        };                                                                        \
        /* Check the requested T, including constness. Concrete methods also need */ \
        /* a valid unqualified T mapping, because clones return owning adapters. */ \
        template<class T> concept cpptraits_has_implementation =                  \
            ::std::is_object_v<T> && !::std::is_volatile_v<T> &&                    \
            cpptraits_implementation_check<T>::value &&                           \
            (!cpptraits_has_concrete ||                                          \
                cpptraits_implementation_check<::std::remove_const_t<T>>::value);                            \
        /* Either an interface implementation or a valid mapping satisfies Name. */ \
        /* A raw mapped T still needs an adapter for interface conversion. */       \
        template<class T> concept cpptraits_satisfied =                           \
            cpptraits_runtime_compatible<T> || cpptraits_has_implementation<T>;   \
        template<class CppTraitsSelf> requires cpptraits_has_implementation<CppTraitsSelf> \
        struct cpptraits_mapped_calls {                                           \
            using CppTraitsImpl = cpptraits_implementation<CppTraitsSelf>;         \
            CPPTRAITS_METHODS(CPPTRAITS_MAPPED_CALL, __VA_ARGS__)                   \
        };                                                                        \
        template<class CppTraitsSelf> requires cpptraits_runtime_compatible<CppTraitsSelf> \
        struct cpptraits_native_calls {                                           \
            CPPTRAITS_METHODS(CPPTRAITS_NATIVE_CALL, __VA_ARGS__)                   \
        };                                                                        \
        CPPTRAITS_CALL_DISPATCH                                                    \
        /* 5: Store an owned value; let the merge helper access it privately. */    \
        template<class T> class cpptraits_owning_adapter                          \
            : public cpptraits_adapter_base<T, cpptraits_owning_adapter<T>> {       \
            template<class, class, class>                                         \
            friend struct ::cpptraits_detail::concrete_adapter;                   \
            using CppTraitsImpl = cpptraits_implementation<T>;                    \
            T object_;                                                            \
            T& cpptraitsObject() noexcept { return object_; }                     \
            const T& cpptraitsObject() const noexcept { return object_; }         \
        public:                                                                   \
            explicit cpptraits_owning_adapter(T object)                           \
                : object_(::std::move(object)) {}                                 \
            CPPTRAITS_METHODS(CPPTRAITS_FORWARD_METHOD, __VA_ARGS__)               \
        };                                                                        \
        /* Borrow through T*: wrapper copies share the same source object. */       \
        /* Keep const on the source, but use the unqualified T's mapping. */        \
        template<class T> class cpptraits_reference_adapter                       \
            : public cpptraits_adapter_base<T, cpptraits_reference_adapter<T>> {   \
            template<class, class, class>                                         \
            friend struct ::cpptraits_detail::concrete_adapter;                   \
            using CppTraitsImpl = cpptraits_implementation<::std::remove_const_t<T>>; \
            T* object_;                                                           \
            T& cpptraitsObject() noexcept { return *object_; }                    \
            const T& cpptraitsObject() const noexcept { return *object_; }        \
        public:                                                                   \
            explicit cpptraits_reference_adapter(T& object) noexcept             \
                : object_(::std::addressof(object)) {}                            \
            cpptraits_reference_adapter(T&&) = delete;                            \
            CPPTRAITS_METHODS(CPPTRAITS_FORWARD_METHOD, __VA_ARGS__)               \
        };                                                                        \
        /* 6: adapt copies lvalues or moves rvalues into an owned value. */          \
        /* Ordinary factories always require a mapping. */                         \
        template<class T> requires                                                \
            cpptraits_has_implementation<::std::remove_cvref_t<T>> &&              \
            ::std::move_constructible<::std::remove_cvref_t<T>> &&                 \
            ::std::constructible_from<::std::remove_cvref_t<T>, T&&>               \
        auto cpptraits_make_value(T&& object) {                                  \
            return cpptraits_owning_adapter<::std::remove_cvref_t<T>>(             \
                ::std::forward<T>(object));                                       \
        }                                                                         \
        /* borrow keeps source constness, does not allocate or extend lifetime. */  \
        template<class T> requires cpptraits_has_implementation<T>                \
        auto cpptraits_make_reference(T& object) noexcept {                       \
            return cpptraits_reference_adapter<T>(object);                       \
        }                                                                         \
        /* Do not allow borrowing from rvalue arguments. */                        \
        template<class T> requires (!::std::is_lvalue_reference_v<T>)              \
        void cpptraits_make_reference(T&&) = delete;                              \
        template<class T> requires                                                \
            cpptraits_has_implementation<::std::remove_cvref_t<T>> &&              \
            ::std::move_constructible<::std::remove_cvref_t<T>> &&                 \
            ::std::constructible_from<::std::remove_cvref_t<T>, T&&>               \
        /* box allocates an owning adapter and returns its interface pointer. */   \
        auto cpptraits_make_box(T&& object)                                       \
            -> ::std::unique_ptr<cpptraits_interface_type> {                       \
            return ::std::make_unique<                                            \
                cpptraits_owning_adapter<::std::remove_cvref_t<T>>>(               \
                    ::std::forward<T>(object));                                   \
        }                                                                         \
    }                                                                             \
    template<class CppTraitsSelf> concept Name =                                  \
        Scope::cpptraits_satisfied<CppTraitsSelf>

// Associated signatures depend on the bindings supplied by a native base or a
// mapping. Process type descriptors in declaration order, skipping methods.
#define CPPTRAITS_ASSOCIATED_TYPES(Generate, ...) \
    CPPTRAITS_EACH(CPPTRAITS_ASSOCIATED_TYPE, Generate, __VA_ARGS__)
#define CPPTRAITS_ASSOCIATED_TYPE(Generate, Descriptor) \
    CPPTRAITS_ASSOCIATED_TYPE_I(Generate, CPPTRAITS_UNPAREN Descriptor)
#define CPPTRAITS_ASSOCIATED_TYPE_I(Generate, ...) CPPTRAITS_ASSOCIATED_TYPE_II(Generate, __VA_ARGS__)
#define CPPTRAITS_ASSOCIATED_TYPE_II(Generate, Kind, Return, Name, ...) \
    CPPTRAITS_ASSOCIATED_TYPE_##Kind(Generate, Name)
#define CPPTRAITS_ASSOCIATED_TYPE_ASSOCIATED(Generate, Name) Generate(Name)
#define CPPTRAITS_ASSOCIATED_TYPE_REQUIRED(Generate, Name)
#define CPPTRAITS_ASSOCIATED_TYPE_DEFAULT(Generate, Name)
#define CPPTRAITS_ASSOCIATED_TYPE_MERGE(Generate, Name)
#define CPPTRAITS_ASSOCIATED_TYPE_CLONE(Generate, Name)

#define CPPTRAITS_ASSOCIATED_PARAMETER(Name) , class CppTraitsBound_##Name
#define CPPTRAITS_ASSOCIATED_BINDING(Name) using Name = CppTraitsBound_##Name;
#define CPPTRAITS_ASSOCIATED_ARGUMENT(Name) , typename CppTraitsSelf::Name
#define CPPTRAITS_ASSOCIATED_MAPPING_ARGUMENT(Name) , typename CppTraitsImpl::Name
#define CPPTRAITS_ASSOCIATED_CHECK(Name) typename CppTraitsImpl::Name;
#define CPPTRAITS_ASSOCIATED_ALIAS(Name) using Name = typename CppTraitsImpl::Name;

#define CPPTRAITS_FORWARD_ASSOCIATED(...)

#define CPPTRAITS_ASSOCIATED_DECLARE_REQUIRED CPPTRAITS_DECLARE_REQUIRED
#define CPPTRAITS_ASSOCIATED_DECLARE_ASSOCIATED(...)
#define CPPTRAITS_ASSOCIATED_DECLARE_UNSUPPORTED(...) \
    static_assert(false, "Associated-type traits support required methods only");
#define CPPTRAITS_ASSOCIATED_DECLARE_DEFAULT CPPTRAITS_ASSOCIATED_DECLARE_UNSUPPORTED
#define CPPTRAITS_ASSOCIATED_DECLARE_MERGE CPPTRAITS_ASSOCIATED_DECLARE_UNSUPPORTED
#define CPPTRAITS_ASSOCIATED_DECLARE_CLONE CPPTRAITS_ASSOCIATED_DECLARE_UNSUPPORTED
#define CPPTRAITS_ASSOCIATED_DECLARE(Kind, ...) CPPTRAITS_ASSOCIATED_DECLARE_##Kind(__VA_ARGS__)

#define CPPTRAITS_ASSOCIATED_SIGNATURE(Return, Parameters, Qualifiers, Self) \
    CPPTRAITS_STATIC_SIGNATURE(cpptraits_contract, Return, Parameters, Qualifiers, Self)
#define CPPTRAITS_ASSOCIATED_REQUIRE_RECEIVER(Return, Parameters, Qualifiers) \
    requires ::std::same_as< \
        CPPTRAITS_ASSOCIATED_SIGNATURE(Return, Parameters, Qualifiers, CppTraitsSelf), \
        CPPTRAITS_ASSOCIATED_SIGNATURE(Return, Parameters, Qualifiers, ::std::remove_const_t<CppTraitsSelf>)>;
#define CPPTRAITS_ASSOCIATED_METHOD_REQUIRED(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    CPPTRAITS_ASSOCIATED_REQUIRE_RECEIVER(Return, Parameters, Qualifiers) \
    static_cast<CPPTRAITS_ASSOCIATED_SIGNATURE(Return, Parameters, Qualifiers, CppTraitsSelf)>( \
        &CppTraitsImpl::Method);
#define CPPTRAITS_ASSOCIATED_METHOD_ASSOCIATED(...)
#define CPPTRAITS_ASSOCIATED_METHOD_DEFAULT(...)
#define CPPTRAITS_ASSOCIATED_METHOD_MERGE(...)
#define CPPTRAITS_ASSOCIATED_METHOD_CLONE(...)
#define CPPTRAITS_ASSOCIATED_METHOD(Kind, ...) CPPTRAITS_ASSOCIATED_METHOD_##Kind(__VA_ARGS__)

#define CPPTRAITS_ASSOCIATED_NATIVE_REQUIRED(Return, Method, Parameters, Arguments, Qualifiers, Body) \
    CPPTRAITS_ASSOCIATED_REQUIRE_RECEIVER(Return, Parameters, Qualifiers) \
    CPPTRAITS_REQUIRE_METHOD(REQUIRED, Return, Method, Parameters, Arguments, Qualifiers, Body)
#define CPPTRAITS_ASSOCIATED_NATIVE_ASSOCIATED(...)
#define CPPTRAITS_ASSOCIATED_NATIVE_DEFAULT(...)
#define CPPTRAITS_ASSOCIATED_NATIVE_MERGE(...)
#define CPPTRAITS_ASSOCIATED_NATIVE_CLONE(...)
#define CPPTRAITS_ASSOCIATED_NATIVE(Kind, ...) CPPTRAITS_ASSOCIATED_NATIVE_##Kind(__VA_ARGS__)

// The first interface argument lets each associated descriptor add a type with
// a leading comma. The public native alias supplies that argument internally.
// Reconstruct the expected base from T's public aliases to catch shadowed types.
#define CPPTRAITS_TRAIT_1(Name, Scope, ...)                                         \
    namespace Scope {                                                             \
        template<class CppTraitsUnused                                            \
            CPPTRAITS_ASSOCIATED_TYPES(CPPTRAITS_ASSOCIATED_PARAMETER, __VA_ARGS__)> \
        struct cpptraits_bound_interface {                                        \
            CPPTRAITS_ASSOCIATED_TYPES(CPPTRAITS_ASSOCIATED_BINDING, __VA_ARGS__)    \
            virtual ~cpptraits_bound_interface() = default;                       \
            CPPTRAITS_METHODS(CPPTRAITS_ASSOCIATED_DECLARE, __VA_ARGS__)            \
        };                                                                        \
        template<class... CppTraitsBindings>                                      \
        using cpptraits_native_interface = cpptraits_bound_interface<void, CppTraitsBindings...>; \
        template<class... CppTraitsBindings>                                      \
        using cpptraits_interface_type = cpptraits_native_interface<CppTraitsBindings...>; \
        template<class T> struct cpptraits_implementation;                        \
        template<class CppTraitsImpl>                                             \
        concept cpptraits_has_associated_types = requires {                       \
            sizeof(CppTraitsImpl);                                                \
            CPPTRAITS_ASSOCIATED_TYPES(CPPTRAITS_ASSOCIATED_CHECK, __VA_ARGS__)      \
        };                                                                        \
        template<class CppTraitsImpl> struct cpptraits_contract {                 \
            CPPTRAITS_ASSOCIATED_TYPES(CPPTRAITS_ASSOCIATED_ALIAS, __VA_ARGS__)      \
            template<class CppTraitsSelf>                                         \
            static constexpr bool mapped = requires {                            \
                CPPTRAITS_METHODS(CPPTRAITS_ASSOCIATED_METHOD, __VA_ARGS__)         \
            };                                                                    \
            template<class CppTraitsSelf>                                         \
            static constexpr bool native = requires {                            \
                CPPTRAITS_METHODS(CPPTRAITS_ASSOCIATED_NATIVE, __VA_ARGS__)         \
            };                                                                    \
            template<class CppTraitsSelf> struct cpptraits_mapped_calls {          \
                CPPTRAITS_METHODS(CPPTRAITS_MAPPED_CALL, __VA_ARGS__)               \
            };                                                                    \
            template<class CppTraitsSelf> struct cpptraits_native_calls {          \
                CPPTRAITS_METHODS(CPPTRAITS_NATIVE_CALL, __VA_ARGS__)               \
            };                                                                    \
            using cpptraits_interface = cpptraits_bound_interface<void            \
                CPPTRAITS_ASSOCIATED_TYPES(CPPTRAITS_ASSOCIATED_MAPPING_ARGUMENT, __VA_ARGS__)>; \
            template<class CppTraitsObject> class cpptraits_owning_adapter        \
                : public cpptraits_interface {                                   \
                CppTraitsObject object_;                                          \
                CppTraitsObject& cpptraitsObject() noexcept { return object_; }   \
                const CppTraitsObject& cpptraitsObject() const noexcept { return object_; } \
            public:                                                               \
                explicit cpptraits_owning_adapter(CppTraitsObject object)         \
                    : object_(::std::move(object)) {}                             \
                CPPTRAITS_METHODS(CPPTRAITS_FORWARD_METHOD, __VA_ARGS__)           \
            };                                                                    \
            template<class CppTraitsObject> class cpptraits_reference_adapter     \
                : public cpptraits_interface {                                   \
                CppTraitsObject* object_;                                         \
                CppTraitsObject& cpptraitsObject() noexcept { return *object_; }  \
                const CppTraitsObject& cpptraitsObject() const noexcept { return *object_; } \
            public:                                                               \
                explicit cpptraits_reference_adapter(CppTraitsObject& object) noexcept \
                    : object_(::std::addressof(object)) {}                        \
                cpptraits_reference_adapter(CppTraitsObject&&) = delete;                        \
                CPPTRAITS_METHODS(CPPTRAITS_FORWARD_METHOD, __VA_ARGS__)           \
            };                                                                    \
        };                                                                        \
        template<class CppTraitsSelf> concept cpptraits_runtime_compatible =       \
            requires {                                                            \
                sizeof(CppTraitsSelf);                                            \
                requires ::std::derived_from<CppTraitsSelf, cpptraits_bound_interface<void \
                    CPPTRAITS_ASSOCIATED_TYPES(CPPTRAITS_ASSOCIATED_ARGUMENT, __VA_ARGS__)>>; \
            } && cpptraits_contract<CppTraitsSelf>::template native<CppTraitsSelf>; \
        template<class T> using cpptraits_mapping =                                \
            cpptraits_implementation<::std::remove_const_t<T>>;                    \
        template<class T> concept cpptraits_has_implementation =                  \
            ::std::is_object_v<T> && !::std::is_volatile_v<T> &&                    \
            cpptraits_has_associated_types<cpptraits_mapping<T>> &&                \
            cpptraits_contract<cpptraits_mapping<T>>::template mapped<T>;          \
        template<class T> concept cpptraits_satisfied =                            \
            !::std::is_volatile_v<T> &&                                            \
            (cpptraits_runtime_compatible<T> || cpptraits_has_implementation<T>);  \
        template<class T> requires cpptraits_has_implementation<T>                \
        using cpptraits_mapped_implementation = cpptraits_mapping<T>;              \
        template<class T> requires cpptraits_has_implementation<T>                \
        using cpptraits_mapped_calls =                                             \
            typename cpptraits_contract<cpptraits_mapping<T>>::template cpptraits_mapped_calls<T>; \
        template<class CppTraitsSelf> requires cpptraits_runtime_compatible<CppTraitsSelf> \
        using cpptraits_native_calls = typename cpptraits_contract<CppTraitsSelf>::template \
            cpptraits_native_calls<CppTraitsSelf>;                                \
        CPPTRAITS_CALL_DISPATCH                                                    \
        template<class T> requires                                                \
            cpptraits_has_implementation<::std::remove_cvref_t<T>> &&              \
            ::std::move_constructible<::std::remove_cvref_t<T>> &&                 \
            ::std::constructible_from<::std::remove_cvref_t<T>, T&&>               \
        auto cpptraits_make_value(T&& object) {                                  \
            using CppTraitsContract = cpptraits_contract<                         \
                cpptraits_mapping<::std::remove_cvref_t<T>>>;                     \
            return typename CppTraitsContract::template cpptraits_owning_adapter< \
                ::std::remove_cvref_t<T>>(::std::forward<T>(object));              \
        }                                                                         \
        template<class T> requires cpptraits_has_implementation<T>                \
        auto cpptraits_make_reference(T& object) noexcept {                       \
            return typename cpptraits_contract<cpptraits_mapping<T>>::template    \
                cpptraits_reference_adapter<T>(object);                          \
        }                                                                         \
        template<class T> requires (!::std::is_lvalue_reference_v<T>)              \
        void cpptraits_make_reference(T&&) = delete;                              \
        template<class T> requires                                                \
            cpptraits_has_implementation<::std::remove_cvref_t<T>> &&              \
            ::std::move_constructible<::std::remove_cvref_t<T>> &&                 \
            ::std::constructible_from<::std::remove_cvref_t<T>, T&&>               \
        auto cpptraits_make_box(T&& object)                                       \
            -> ::std::unique_ptr<typename cpptraits_contract<                     \
                cpptraits_mapping<::std::remove_cvref_t<T>>>::cpptraits_interface> { \
            using CppTraitsContract = cpptraits_contract<                         \
                cpptraits_mapping<::std::remove_cvref_t<T>>>;                     \
            return ::std::make_unique<typename CppTraitsContract::template        \
                cpptraits_owning_adapter<::std::remove_cvref_t<T>>>(               \
                    ::std::forward<T>(object));                                   \
        }                                                                         \
    }                                                                             \
    template<class CppTraitsSelf> concept Name = Scope::cpptraits_satisfied<CppTraitsSelf>

#define trait(Name, ...) \
    CPPTRAITS_CAT(CPPTRAITS_TRAIT_, CPPTRAITS_HAS_ASSOCIATED(__VA_ARGS__))( \
        Name, Name##_cpptraits_detail, __VA_ARGS__)

// Define an ordinary C++20 concept, with no runtime interface or adapters.
// The variadic constraint allows commas, such as in std::same_as<T, int>.
#define static_trait(Name, T, ...) \
    template<typename T> concept Name = __VA_ARGS__

// Build the component checks and interface list one item at a time.
// Each check adds '&& Trait<T>'; each type adds ', TraitInterface'.
#define CPPTRAITS_COMPONENT_CHECK(T, Component) && Component<T>
#define CPPTRAITS_COMPONENT_TYPE(Unused, Component) , Component##_cpptraits_detail::cpptraits_interface_type

// ---- Compose traits: require all components and provide a view of them. ----
// The concept starts with true so every component can append an && check.
// Views use the generated interface list; they do not generate combined methods.
//
// A mapped type can satisfy every component without inheriting their interfaces.
// Factories therefore also check that a view can bind to the source's existing
// interface bases. Reject such calls through constraints, not errors in the body.
// This does not create component adapters from mappings or support nested bundles.
//
// Borrowing a const object selects the read-only view. adapt/box store one value;
// owned_bundle binds and maintains the pointers to that value's interfaces.
#define composed_trait(Name, T, ...)                                              \
    template<typename T>                                                          \
    concept Name = true CPPTRAITS_EACH(CPPTRAITS_COMPONENT_CHECK, T, __VA_ARGS__);   \
    namespace Name##_cpptraits_detail {                                           \
        /* Define mutable and read-only views of the same component interfaces. */ \
        using cpptraits_interface_type = ::cpptraits_detail::interface_view<false            \
            CPPTRAITS_EACH(CPPTRAITS_COMPONENT_TYPE, _, __VA_ARGS__)>;              \
        using cpptraits_const_interface_type = ::cpptraits_detail::interface_view<true       \
            CPPTRAITS_EACH(CPPTRAITS_COMPONENT_TYPE, _, __VA_ARGS__)>;              \
        static_assert(::std::has_virtual_destructor_v<cpptraits_interface_type>,             \
            "Composed bundles require safe polymorphic destruction");             \
        /* Borrow only from objects whose component interfaces can be bound. */   \
        template<typename CppTraitsObject>                                        \
            requires Name<::std::remove_cv_t<CppTraitsObject>> &&                   \
                (!::std::is_volatile_v<CppTraitsObject>) &&                        \
                ::std::constructible_from<::std::conditional_t<                   \
                    ::std::is_const_v<CppTraitsObject>,                           \
                    cpptraits_const_interface_type, cpptraits_interface_type>,    \
                    CppTraitsObject&>                                             \
        auto cpptraits_make_reference(CppTraitsObject& object) noexcept {                    \
            if constexpr (::std::is_const_v<CppTraitsObject>) {                    \
                return cpptraits_const_interface_type(object);                              \
            } else {                                                              \
                return cpptraits_interface_type(object);                                    \
            }                                                                     \
        }                                                                         \
        template<typename CppTraitsObject>                                        \
            requires (!::std::is_lvalue_reference_v<CppTraitsObject>)               \
        void cpptraits_make_reference(CppTraitsObject&&) = delete;                           \
        template<typename CppTraitsObject>                                        \
            requires Name<::std::remove_cvref_t<CppTraitsObject>> &&                \
                ::std::constructible_from<cpptraits_interface_type,               \
                    ::std::remove_cvref_t<CppTraitsObject>&> &&                    \
                ::std::move_constructible<::std::remove_cvref_t<CppTraitsObject>> && \
                ::std::constructible_from<::std::remove_cvref_t<CppTraitsObject>,   \
                                          CppTraitsObject&&>                      \
        auto cpptraits_make_value(CppTraitsObject&& object) {                                \
            return ::cpptraits_detail::owned_bundle<                               \
                ::std::remove_cvref_t<CppTraitsObject>                             \
                CPPTRAITS_EACH(CPPTRAITS_COMPONENT_TYPE, _, __VA_ARGS__)>(          \
                    ::std::forward<CppTraitsObject>(object));                     \
        }                                                                         \
        template<typename CppTraitsObject>                                        \
            requires Name<::std::remove_cvref_t<CppTraitsObject>> &&                \
                ::std::constructible_from<cpptraits_interface_type,               \
                    ::std::remove_cvref_t<CppTraitsObject>&> &&                    \
                ::std::move_constructible<::std::remove_cvref_t<CppTraitsObject>> && \
                ::std::constructible_from<::std::remove_cvref_t<CppTraitsObject>,   \
                                          CppTraitsObject&&>                      \
        auto cpptraits_make_box(CppTraitsObject&& object) -> ::std::unique_ptr<cpptraits_interface_type> {      \
            return ::std::make_unique<::cpptraits_detail::owned_bundle<            \
                ::std::remove_cvref_t<CppTraitsObject>                             \
                CPPTRAITS_EACH(CPPTRAITS_COMPONENT_TYPE, _, __VA_ARGS__)>>(          \
                    ::std::forward<CppTraitsObject>(object));                     \
        }                                                                         \
    }

// ---- Public entry points. ----
// implements(Name, ...) supplies a public interface base. Bind associated types
// in declaration order. The caller supplies ':'.
// Merge/clone traits require impl instead, because their mappings name concrete T.
// impl(Name, Type) defines a full specialization. The three-argument form
// supplies a parenthesized template declaration and target type for a partial
// specialization. Declare either form in the trait's namespace or an enclosing
// namespace, as C++ specialization requires.
#define implements(Name, ...) public Name##_cpptraits_detail::cpptraits_native_interface<__VA_ARGS__>
#define CPPTRAITS_IMPL_EXACT(Scope, Type) \
    template<> struct Scope::cpptraits_implementation<CPPTRAITS_UNPAREN Type>
#define CPPTRAITS_IMPL_TEMPLATE(Scope, Declaration, Type) \
    CPPTRAITS_TYPE(CPPTRAITS_UNPAREN Declaration) \
    struct Scope::cpptraits_implementation<CPPTRAITS_TYPE(Type)>
#define CPPTRAITS_SELECT_IMPL(Selected, ...) Selected
// Keep the second argument grouped when a type macro expands to tokens with commas.
#define impl(Name, TypeOrDeclaration, ...) \
    CPPTRAITS_SELECT_IMPL(__VA_OPT__(CPPTRAITS_IMPL_TEMPLATE,) CPPTRAITS_IMPL_EXACT)( \
        Name##_cpptraits_detail, (TypeOrDeclaration) __VA_OPT__(, __VA_ARGS__))

// Access a validated mapping type, or call a required operation on a native or
// mapped receiver. Native members take precedence when both paths are valid.
#define trait_mapping_t(Name, ...) Name##_cpptraits_detail::cpptraits_mapped_implementation<__VA_ARGS__>
#define trait_call(Name, Method, Object, ...) \
    Name##_cpptraits_detail::cpptraits_calls<::std::remove_cvref_t<decltype((Object))>>::Method( \
        (Object) __VA_OPT__(, __VA_ARGS__))

// dyn names the generated interface type; it does not create an object.
// Associated traits take bindings in declaration order: dyn(Iterator, int).
// Qualified names work: dyn(app::Rotatable) selects the interface in
// app::Rotatable_cpptraits_detail.
// adapt/borrow/box call the generated factories, which handle checks and ownership.
// Variadic arguments preserve commas in expressions like box(Trait, T{a, b});
// the source expression is evaluated once. The same macros work for compositions.
#define dyn(Name, ...) Name##_cpptraits_detail::cpptraits_interface_type __VA_OPT__(<__VA_ARGS__>)
#define adapt(Name, ...) Name##_cpptraits_detail::cpptraits_make_value(__VA_ARGS__)
#define borrow(Name, ...) Name##_cpptraits_detail::cpptraits_make_reference(__VA_ARGS__)
#define box(Name, ...) Name##_cpptraits_detail::cpptraits_make_box(__VA_ARGS__)
