#pragma once
#include "./indexmap/idx_map.hpp"
#include "./storage/handle.hpp"
#include "./concepts.hpp"
#include "./assignment.hpp"
#include "./iterator_adapter.hpp"

namespace nda {

  // Memory Policy
  enum class mem_policy_e { Borrowed, Shared };

  // Const or Mutable
  enum class cm_e { Const, Mutable };

  // forward
  template <typename ValueType, int Rank> class array;

  // detects ellipsis in a argument pack
  template <typename... T> constexpr bool ellipsis_is_present = ((std::is_same_v<T, ellipsis> ? 1 : 0) + ... + 0); // +0 because it can be empty

  // ---------------------- array_view  --------------------------------

  // Try to put the const/mutable in the TYPE

  //template <typename ValueType, int Rank, cm_e ConstMutable = Mutable, mem_policy_e MemPolicy = Borrowed>
  template <typename ValueType, int Rank, mem_policy_e MemPolicy = mem_policy_e::Borrowed>
  class array_view : tag::concepts::_array, tag::containers::_array_view {
    //static_assert(!std::is_const<ValueType>::value, "no const type");

    public:
    using value_t                  = std::remove_const_t<ValueType>;
    using value_as_template_arg_t  = ValueType;

    static constexpr mem_policy_e mem_policy = MemPolicy;
    using storage_t                          = mem::handle<value_t, (mem_policy == mem_policy_e::Shared ? 'S' : 'B')>;
    using idx_map_t                          = idx_map<Rank>;

    using regular_t    = array<value_t, Rank>;
    using view_t       = array_view<value_t, Rank>;
    using const_view_t = array_view<value_t const, Rank>;

    static constexpr int rank = Rank;
    static constexpr bool is_view = true;
    static constexpr bool is_const = std::is_const_v<ValueType>;

    // FIXME : h5
    // static std::string hdf5_scheme() { return "array<" + triqs::h5::get_hdf5_scheme<value_t>() + "," + std::to_string(rank) + ">"; }

    private:

    template<int R> using my_view_template_t = array_view<value_t, R>;
    
    idx_map_t _idx_m;
    storage_t _storage;

    public:
    // ------------------------------- constructors --------------------------------------------

    /// Construct an empty view.
    array_view() = default;

    ///
    array_view(array_view &&) = default;

    /// Shallow copy. It copies the *view*, not the data.
    array_view(array_view const &) = default;

    /** 
     * Construct a view of T const from a view of T
     * @param v a view 
     *
     * NB : Only valid when ValueType is const
     */
    array_view(array_view<value_t, Rank> const &v) REQUIRES(is_const) : array_view(v.indexmap(), v.storage()) {}

    /**
     *  [Advanced] From an indexmap and a storage handle
     *  @param idx index map
     *  @st  storage (memory handle)
     */
    array_view(idx_map<Rank> idx, storage_t st) : _idx_m(std::move(idx)), _storage(std::move(st)) {}

    /** 
     * From anything that has an indexmap and a storage compatible with this class
     * @tparam T an array/array_view or matrix/vector type
     * @param a array or view
     *
     * NB : short cut for array_view (x.indexmap(), x.storage())
     * Allows cross construction of array_view from matrix/matrix view e.g.
     */
    template <typename T> explicit array_view(T const &a) : array_view(a.indexmap(), a.storage()) {}

    // Move assignment not defined : will use the copy = since view must copy data

    // ------------------------------- assign --------------------------------------------

    /**
     * @tparam RHS Can be 
     * 	              - an object modeling the concept NDArray
     * 	              - a type from which ValueType is constructible
     * @param rhs
     *
     * Copies the content of rhs into the view.
     * Pseudo code : 
     *     for all i,j,k,l,... : this[i,j,k,l] = rhs(i,j,k,l)
     *
     * The dimension of RHS must be large enough or behaviour is undefined.
     * 
     * If NDA_BOUNDCHECK is defined, the bounds are checked.
     */
    template <typename RHS> array_view &operator=(RHS const &rhs) {
      nda::details::assignment(*this, rhs);
      return *this;
    }

    /// A special case of the general operator
    /// [C++ oddity : this case must be explicitly coded too]
    array_view &operator=(array_view const &rhs) {
      nda::details::assignment(*this, rhs);
      return *this;
    }

    // ------------------------------- rebind --------------------------------------------

    /// Rebind the view
    void rebind(array_view<value_t, Rank> const &X) {
      _idx_m   = X._idx_m;
      _storage = X._storage;
    }

    /// Rebind
    void rebind(array_view<value_t const, Rank> const &X) REQUIRES(!is_const) { // REQUIRES otherwise it is the same function
      _idx_m   = X._idx_m;
      _storage = X._storage;
    }
    //check https://godbolt.org/z/G_QRCU

    //----------------------------------------------------
    
#include "./_regular_view_common.hpp"
  };

  /// Aliases
  template <typename ValueType, int Rank, mem_policy_e MemPolicy> using array_const_view = array_view<ValueType const, Rank, MemPolicy>;

} // namespace nda