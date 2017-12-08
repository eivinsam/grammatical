#pragma once

#include <iterator>
#include <optional>

namespace ranged
{

	template <class IT>
	struct InputInteratorTraits
	{
		using iterator_category = std::input_iterator_tag;
		using difference_type = void;

		using pointer   = decltype(std::declval<IT>().operator->());
		using reference = decltype(std::declval<IT>().operator*());
		using value_type = std::remove_reference_t<reference>;
	};

	template <class IT, class S>
	class RangeBase
	{
		IT _first;
		S _last;
	public:
		using size_type = void;
		using value_type = typename std::iterator_traits<IT>::value_type;

		RangeBase(IT first, S last) : _first(std::move(first)), _last(std::move(last)) { }

		IT begin() const { return _first; }
		S end() const { return _last; }

		bool empty() const { return _first == _last; }

		template <class T, class A>
		operator std::vector<T, A>() const
		{
			std::vector<T, A> result;
			for (auto&& e : *this)
				result.emplace_back(e);
			return result;
		}
	};

	template <class IT, class S = IT, class Category = typename std::iterator_traits<IT>::iterator_category>
	class Range : public RangeBase<IT, S>
	{
	public:
		using RangeBase<IT, S>::RangeBase;
	};

	template <class IT, class S>
	class Range<IT, S, std::random_access_iterator_tag> : public RangeBase<IT, S>
	{
	public:
		using size_type = typename std::iterator_traits<IT>::difference_type;

		using RangeBase<IT, S>::RangeBase;
		using RangeBase<IT, S>::begin;
		using RangeBase<IT, S>::end;

		size_type size() const { return end() - begin(); }
	};

	template <class IT, class S>
	Range<IT, S> range(IT first, S last) { return { std::move(first), std::move(last) }; }

	template <class F>
	struct Mapping
	{
		F function;
	};

	template <class F>
	Mapping<std::decay_t<F>> map(F&& f) { return { std::forward<F>(f) }; }

	static constexpr struct {} keys = {};
	static constexpr struct {} values = {};

	template <class IT, class F>
	class MappedIterator
	{
		IT _it;
		F _f;
		std::optional<std::decay_t<decltype(std::declval<F>()(*std::declval<IT>()))>> _value;
	public:
		using iterator_category = std::input_iterator_tag;
		using difference_type = typename std::iterator_traits<IT>::difference_type;
		using value_type = const std::decay_t<decltype(*_value)>;
		using reference = value_type&;
		using pointer = value_type*;

		MappedIterator(IT it, F f) : _it(std::move(it)), _f(std::move(f)) { }

		MappedIterator& operator++() { ++_it; _value = std::nullopt; return *this; }

		reference operator*() { if (!_value) _value = _f(*_it); return  *_value; }
		pointer  operator->() { if (!_value) _value = _f(*_it); return &*_value; }

		template <class S> bool operator==(S&& b) const { return _it == std::forward<S>(b); }
		template <class S> bool operator!=(S&& b) const { return _it != std::forward<S>(b); }
	};


	template <class T, class Enable = void>
	struct DeduceRange { };

	template <class T>
	struct DeduceRange<T, std::void_t<decltype(std::begin(std::declval<T>()))>>
	{
		using type = decltype(std::begin(std::declval<T>()));

		static decltype(auto) begin(T& a) { return std::begin(a); }
		static decltype(auto) end(T& a) { return std::end(a); }
	};
	template <class IT>
	struct DeduceRange<std::pair<IT, IT>, std::void_t<typename std::iterator_traits<IT>::iterator_category>>
	{
		using T = std::pair<IT, IT>;
		using type = IT;
	
		static const type& begin(T& a) { return a.first; }
		static const type& end(T& a) { return a.second; }
	};

	template <class C>
	using Iterator = typename DeduceRange<C>::type;
	template <class C>
	decltype(auto) begin(C&& c) { return DeduceRange<C>::begin(c); }
	template <class C>
	decltype(auto) end(C&& c) { return DeduceRange<C>::end(c); }

	template <class C, class IT = Iterator<C>>
	auto operator|(C&& c, decltype(keys))
	{
		return std::forward<C>(c) | map([](auto&& kv) { return kv.first; });
	}
	template <class C, class IT = Iterator<C>>
	auto operator|(C&& c, decltype(values))
	{
		return std::forward<C>(c) | map([](auto&& kv) { return kv.second; });
	}

	template <class C, class F, class IT = Iterator<C>>
	auto operator|(C&& c, Mapping<F> map)
	{
		return range(MappedIterator<IT, F>(begin(std::forward<C>(c)), std::move(map.function)), end(std::forward<C>(c)));
	}
}
