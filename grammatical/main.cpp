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
			new_word->errors.emplace_back("unknown word " + *_it);
			result.emplace_back(move(new_word));
		}
		++_it;
		return result;
	}
};

#include <oui_window.h>
#include <oui_text.h>


class PhraseDrawer
{
	oui::Point pen;
	oui::VectorFont& font;
	int depth = 0;

	float add_space()
	{
		const float half_space_width = font.offset(" ", 24)*0.5f;
		pen.x += half_space_width;
		float mid_x = pen.x;
		pen.x += half_space_width;
		return mid_x;
	}

	struct DecreaseOnDestruct
	{
		int& value;

		~DecreaseOnDestruct() { --value; }
	};
public:
	PhraseDrawer(oui::Point pen, oui::VectorFont& font) : pen(pen), font(font) { }

	int draw(const Phrase& p)
	{
		depth += 1;
		DecreaseOnDestruct decr{ depth };

		if (auto lp = dynamic_cast<const LeftBranch*>(&p))
		{
			int max_depth = draw(*lp->mod);
			const auto mid_x = add_space();
			max_depth = std::max(max_depth, draw(*lp->head));
			oui::Point bottom = { mid_x, pen.y + (max_depth - depth) * 24 };
			oui::line({ mid_x, pen.y }, bottom);
			auto typestr = std::string(1, lp->type);
			bottom.x -= font.offset(typestr, 24)*0.5;
			font.drawLine(bottom, typestr, 24, oui::colors::black);
			return max_depth;
		}
		if (auto rp = dynamic_cast<const RightBranch*>(&p))
		{
			int max_depth = draw(*rp->head);
			const auto mid_x = add_space();
			max_depth = std::max(max_depth, draw(*rp->mod));
			oui::Point bottom = { mid_x, pen.y + (max_depth - depth) * 24 };
			oui::line({ mid_x, pen.y }, bottom);
			auto typestr = std::string(1, rp->type);
			bottom.x -= font.offset(typestr, 24)*0.5;
			font.drawLine(bottom, typestr, 24, oui::colors::black);
			return max_depth;
		}
		pen.x = font.drawLine(pen, p.toString(), 24, oui::colors::black);
		return depth;
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
		"might they have been invited to the party",
		"they eat garf",
		"they give me books",
		"he give me books"
	};

	auto window = oui::Window({ "grammatical", 600, 300, 4 });
	auto font = oui::VectorFont(oui::resolve(oui::NativeFont::serif));

	std::vector<std::vector<Phrase::ptr>> phrases;

	for (auto sentence : sentences)
	{
		Tokenizer<std::istringstream> tokens(sentence);

		Parser parse;

		while (auto word = tokens.next())
			parse.push(*word);

		for (auto&& result : parse.run())
		{
			phrases.push_back(move(result));
			//for (auto&& phrase : result)
			//	cout << ' ' << phrase->toString();
			//cout << '\n';
			//for (auto&& phrase : result)
			//	for (auto&& error : phrase->errors)
			//		cout << "  * " << error << '\n';
		}
	}

	size_t index = 0;
	oui::input.keydown = [&](oui::Key key, oui::PrevKeyState prev_state)
	{
		if (prev_state != oui::PrevKeyState::up)
			return;
		switch (key)
		{
		case oui::Key::left: if (index > 0) --index; window.redraw(); break;
		case oui::Key::right: if (index < phrases.size() - 1) ++index; window.redraw(); break;
		}
	};

	while (window.update())
	{

		auto area = window.area();
		window.clear(oui::colors::white);

		PhraseDrawer(area.min, font).draw(*phrases[index].front());
		
		

	}

	return 0;
}

