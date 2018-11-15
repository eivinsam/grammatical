#include "phrase.h"
#include "tokens.h"
#include "parser.h"

#include <iostream>

using std::cout;
using std::make_shared;
using std::forward;
using std::shared_ptr;
using std::vector;
using std::optional;


template <class Stream>
class Tokenizer
{
	TokenIterator<Stream> _it;
public:
	template <class... Args>
	Tokenizer(Args&&... args) : _it(forward<Args>(args)...) { }

	optional<vector<Phrase::ptr>> next()
	{
		if (_it.isWhitespace()) ++_it;
		if (!_it || _it.isNewline())
			return std::nullopt;
		auto result = parse_word(*_it);
		if (result.empty())
		{
			const auto new_morph = make_shared<Morpheme>(*_it);
			auto new_word = make_shared<Word>(new_morph->sem, new_morph);
			new_word->errors.emplace("unknown word " + *_it);
			result.emplace_back(move(new_word));
		}
		++_it;
		return result;
	}
};


int main(int argc, char* argv[])
{
	const char* sentences[] =
	{
		"have arrived",
		"they will come",
		"the book",
		"that English teacher",
		"a wish",
		"my latest idea",
		"computers are very expensive",
		"do you sell old books",
		"we ate a lot of food",
		"we bought some new furniture",
		"that is useful information",
		"he gave me some useful advice",
		"they gave us a lot of information",
		"let me give you some advice",
		"let me give you a piece of advice",
		"that is a useful piece of equipment",
		//"we bought a few bits of furniture for the new apartment", // 'a few' is unsolved
		//"how much luggage have you got", // question pronouns not yet supported
		"she had six separate items of luggage",
		"everybody is watching",
		"is everybody watching",
		"they had worked hard",
		"had they worked hard",
		"he has finished work",
		"has he finished work",
		"everybody had been working hard",
		"had everybody been working hard",
		"will they come",
		"he might come",
		"might he come",
		"they will have arrived by now",
		"will they have arrived by now",
		"she would have been listening",
		"would she have been listening",
		"the work will be finished soon",
		"will the work be finished soon",
		"they might have been invited to the party",
		"might they have been invited to the party"
	};

	int i = 0;
	for (auto sentence : sentences)
	{
		++i;
		Tokenizer<std::istringstream> tokens(sentence);

		Parser parse;

		while (auto word = tokens.next())
			parse.push(*word);

		for (auto&& result : parse.run())
		{
			cout << i << ":";
			for (auto&& phrase : result)
				cout << ' ' << phrase->toString();
			cout << '\n';
			for (auto&& phrase : result)
				for (auto&& error : phrase->errors)
					cout << "  * " << error << '\n';
		}
	}

	return 0;
}

