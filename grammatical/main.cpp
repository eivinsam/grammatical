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
			const auto new_morph = make_shared<Morpheme>(make_shared<Lexeme>(*_it));
			auto new_word = make_shared<Word>(new_morph->lex, new_morph);
			new_word->errors.emplace("unknown word " + *_it);
			result.emplace_back(move(new_word));
		}
		++_it;
		return result;
	}
};

// 'I  -> noun, sg, 1, nom
// 'me -> noun, sg, 1, acc
// 'my -> noun, sg, 1, gen
// ...
// 'cow -> noun, 3, nom, acc, :sg(gen)
// 'cows -> noun, 3, nom, acc, pl, :pl(gen)
// 'milk -> noun, 3, nom, acc, sg, :(gen)

// X(Y) + Z:X(Y) -> Z.X

// gen.sg + noun:gen.sg -> noun.sg
// gen.pl + noun:gen.pl -> noun.pl
// noun.nom.nonsg3 + verb.pres:nom.nonsg3 -> verb.pres.s
// noun.nom.sg.3 + verb.pres:nom.sg.3 -> verb.pres.s
// noun.nom.num.per + verb.past:nom -> verb.past.s
// noun.nom.num.par + verb.part:nom -> verb.part.s
// 

int main(int argc, char* argv[])
{
	const char* sentences[] =
	{
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
		"we bought a few bits of furniture for the new apartment",
		//"how much luggage have you got",
		"she had six separate items of luggage",
		"everybody is watching",
		"is everybody watching",
		"they had worked hard",
		"had they worked hard",
		"he has finished work",
		"has he finished work",
		"everybody had been working hard",
		"had everybody been working hard"
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

