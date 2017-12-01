#include "phrase.h"

#include <string_view>

using std::pair;
using std::shared_ptr;
using std::vector;
using std::string_view;

static bool ignore_case_less(string_view a, string_view b)
{
	auto ita = a.begin(); const auto enda = a.end();
	auto itb = b.begin(); const auto endb = b.end();
	for (; ita != enda; ++ita, ++itb)
	{
		if (itb == endb) return false;
		const auto ca = tolower(*ita);
		const auto cb = tolower(*itb);
		if (cb < ca) return false;
		if (ca < cb) return true;
	}
	return itb != endb;
}


const vector<shared_ptr<const Word>>& parse_word(string_view orth)
{
	class Trie
	{
		using Entry = shared_ptr<const Word>;
		vector<Entry>  _matches;
		vector<pair<char, Trie>> _subs;

		auto _find(char ch) const
		{
			for (auto it = _subs.begin(); it != _subs.end(); ++it)
				if (it->first == ch)
					return it;
			return _subs.end();
		}
		auto _find(char ch)
		{
			for (auto it = _subs.begin(); it != _subs.end(); ++it)
				if (it->first == ch)
					return it;
			return _subs.end();

		}
	public:
		Trie() = default;
		Trie(std::initializer_list<pair<string_view, Tags>> entries)
		{
			for (auto&& e : entries)
				insert(e.first, std::make_shared<Word>(e.first, e.second));
		}

		void insert(string_view orth, Entry entry)
		{
			if (orth.empty())
				_matches.emplace_back(move(entry));
			else
			{
				const char key = tolower(orth.front());
				auto found = _find(key);
				if (found == _subs.end())
				{
					_subs.emplace_back();
					_subs.back().first = key;
					found = --_subs.end();
				}
				found->second.insert(orth.substr(1), move(entry));
			}
		}
		const vector<Entry>& find(string_view orth) const
		{
			static const vector<Entry> empty;
			if (orth.empty())
				return _matches;
			else
			{
				const char key = tolower(orth.front());
				auto found = _find(key);
				if (found == _subs.end())
					return empty;
				return found->second.find(orth.substr(1));
			}
		}
	};

	static constexpr Tags nsg3 = Tag::noun | Tag::sg | Tag::third;
	static constexpr Tags npl3 = Tag::noun | Tag::pl | Tag::third | Tag::spec;
	static constexpr Tags vsgpres = Tag::verb | Tag::finite | Tag::sg;
	static constexpr Tags vplpres = Tag::verb | Tag::finite | Tag::pl;
	static constexpr Tags vpast   = Tag::verb | Tag::finite | Tag::past;
	static constexpr Tags vpresp = Tag::verb | Tag::part;
	static constexpr Tags vpastp = Tag::verb | Tag::part | Tag::past;

	static const Trie dict =
	{
		{ "a", Tag::det | Tag::sg },
		{ "an", Tag::det | Tag::sg },
		{ "some", Tag::det | Tag::pl | Tag::uc },
		{ "the", Tag::det | Tag::sg | Tag::pl },
		{ "this", Tag::det | Tag::sg }, { "these", Tag::det | Tag::pl },
		{ "that", Tag::det | Tag::sg }, { "those", Tag::det | Tag::pl },
		{ "my", Tag::det | Tag::sg | Tag::pl },
		{ "have", vplpres | Tag::aux_past },
		{ "has", vsgpres | Tag::aux_past },
		{ "having", vpresp | Tag::aux_past },
		{ "had", vpast | vpastp | Tag::aux_past },
		{ "do", vplpres }, 
		{ "does", vsgpres },
		{ "doing", vpresp },
		{ "did", vpast },
		{ "done", vpastp },
		{ "sell", vplpres }, { "sells", vsgpres }, { "selling", vpresp }, { "sold", vpast | vpastp },
		{ "book", nsg3 }, { "books", npl3 },
		{ "idea", nsg3 }, { "ideas", npl3 },
		{ "teacher", nsg3 }, { "teachers", npl3 },
		{ "wish", nsg3 }, { "wishes", npl3 },
		{ "English", Tag::adn },
		{ "expensive", Tag::adn },
		{ "latest", Tag::adn },
		{ "old", Tag::adn },
		{ "very", Tag::adad }
	};

	return dict.find(orth);
}
