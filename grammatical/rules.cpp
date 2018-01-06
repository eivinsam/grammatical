#include "phrase.h"

std::shared_ptr<LeftBranch> merge(Lexeme::ptr lex, const Mod& mod, char type, const Head& head, LeftRule l, RightRule r)
{
	return std::make_shared<LeftBranch>(move(lex), type, head, mod, l, r);
}

std::shared_ptr<RightBranch> merge(Lexeme::ptr lex, const Head& head, char type, const Mod& mod, LeftRule l, RightRule r)
{
	return std::make_shared<RightBranch>(move(lex), type, head, mod, l, r);
}

template <class A, class B>
decltype(auto) append(A&& a, B&& b)
{
	a.insert(a.end(), b.begin(), b.end());
	return std::forward<A>(a);
}

RuleOutput noun_det(const Mod& mod, const Head& head)
{
	if (mod->is(Tag::gen))
	{
		const auto result = merge(head->lex, mod, ':', head, no_left, no_right);

		if (!mod->lex->matches<Rel::spec>(head->lex))
			result->errors.emplace("det and noun not compatible");
		return { result };
	}
	return {};
}

RuleOutput ad_adad(const Mod& mod, const Head& head)
{
	if (mod->is(Tag::adad))
		return { merge(head->lex, mod, '>', head, no_left, no_right) };
	return {};
}

RuleOutput noun_adjective(const Mod& mod, const Head& head)
{
	if (mod->is(Tag::adn))
		return { merge(head->lex, mod, '>', head, noun_adjective, no_right) };
	return noun_det(mod, head);
}

RuleOutput head_prep(const Head& head, const Mod& mod)
{
	if (mod->is(Tag::prep))
	{
		const auto result = merge(head->lex, head, '<', mod, head->left_rule, head_prep);
		if (!mod->lex->matches<Rel::mod>(head->lex))
			result->errors.emplace("preposition not agreeable");
		if (!mod->hasBranch('+'))
			result->errors.emplace("preposition has no complement");
		return { result };
	}
	return {};
}

RuleOutput noun_rmod(const Head& head, const Mod& mod)
{
	if (mod->is(Tag::part))
	{
		const auto result = merge(head->lex, head, '<', mod, noun_adjective, no_right);

		if (!mod->is(Tag::part))
			result->errors.emplace("verb right-modifying noun must be a participle");
		else
		{
			if (mod->is(Tag::past) && mod->hasBranch('+'))
				result->errors.emplace("past participle modifying noun can't have an object");
			if (mod->is(Tag::pres) && mod->hasBranch(':'))
				result->errors.emplace("present participle modifying noun can't have subject");
			if (dynamic_cast<const Word*>(mod.get()))
				result->errors.emplace("verb phrase must be complex to right-modify a noun");
		}

		return { result };
	}
	return head_prep(head, mod);
}

RuleOutput verb_spec(const Mod& mod, const Head& head)
{
	if (mod->is(Tag::nom))
	{
		const auto result = merge(head->lex, mod, ':', head, no_left, no_right);

		if (!mod->lex->matches<Rel::spec>(head->lex))
			result->errors.emplace("verb-subject disagreement");

		return { result };
	}
	return {};
}
RuleOutput head_comp(const Head& head, const Mod& mod, RightRule next_right)
{
	auto result = next_right(head, mod);
	if (mod->is(Tag::akk) || mod->is(Tag::fin) || mod->is(Tag::part) || mod->is(Tag::adn))
	{
		const auto match = merge(head->lex, head, '+', mod, head->left_rule, next_right);

		if (!mod->lex->matches<Rel::comp>(head->lex))
			match->errors.emplace("verb-object disagreement");

		if ((mod->is(Tag::fin) || mod->is(Tag::part)) && mod->hasBranch(':'))
			match->errors.emplace("verbal object cannot have a subject");

		result.emplace_back(match);
	}
	return result;
}
RuleOutput prep_comp(const Head& head, const Mod& mod) { return head_comp(head, mod, no_right); }

RuleOutput verb_adv(const Head& head, const Mod& mod)
{
	if (mod->is(Tag::adv))
	{
		const auto match = merge(head->lex, head, '<', mod, head->left_rule, head_prep);
		if (!mod->lex->matches<Rel::mod>(head->lex))
			match->errors.emplace("verb not known to take this adverb");
		return { match };
	}
	return head_prep(head, mod);
}

RuleOutput verb_comp(const Head& head, const Mod& mod)
{
	return head_comp(head, mod, verb_adv);
}
RuleOutput verb_bicomp(const Head& head, const Mod& mod)
{
	auto result = verb_comp(head, mod);
	if (mod->is(Tag::akk))
	{
		const auto match = merge(head->lex, head, '*', mod, head->left_rule, verb_comp);

		if (!mod->lex->matches<Rel::bicomp>(head->lex))
			match->errors.emplace("verb - indirect object disagreement");

		result.emplace_back(match);
	}
	return result;
}
RuleOutput verb_rspec(const Head& head, const Mod& mod)
{
	auto result = verb_bicomp(head, mod);
	if (mod->is(Tag::nom))
	{
		const auto match = merge(head->lex, head, ':', mod, no_left, verb_bicomp);
		
		if (!mod->lex->matches<Rel::spec>(head->lex))
			match->errors.emplace("noun-verb number/person disagreement");

		result.emplace_back(match);
	}
	return result;
}


Word::Word(Lexeme::ptr lexeme, Phrase::ptr morph) : Phrase{ 1, move(lexeme),{} }, _morph{ move(morph) }
{
	if (lex->is(Tag::nom) || lex->is(Tag::akk))
	{
		left_rule = noun_adjective;
		right_rule = noun_rmod;
	}
	else if (lex->is(Tag::fin) || lex->is(Tag::part))
	{
		left_rule = lex->is(Tag::fin) ? verb_spec : no_left;
		right_rule = lex->is(Tag::free) ? verb_rspec : verb_bicomp;
	}
	else if (lex->is(Tag::adn))
	{
		left_rule = ad_adad;
	}
	else if (lex->is(Tag::prep))
	{
		left_rule = no_left;
		right_rule = prep_comp;
	}
}

RuleOutput noun_suffix(const Head& head, const Mod& mod)
{
	if (mod->is(Tag::suffix))
	{
		if (mod->lex->name == "s")
		{
			const auto match = merge(head->lex, head, '-', mod, no_left, no_right);
			return { match };
		}
	}
	return {};
}

Morpheme::Morpheme(Lexeme::ptr lexeme) : Phrase{ lexeme->name.size(), lexeme, {} }
{
	if (lex->is(Tag::rc))
	{
		right_rule = noun_suffix;
	}
}
