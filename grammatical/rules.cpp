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
	if (mod->is(tag::gen))
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
	if (mod->is(tag::adad))
		return { merge(head->lex, mod, '>', head, no_left, no_right) };
	return {};
}

RuleOutput noun_adjective(const Mod& mod, const Head& head)
{
	if (mod->is(tag::adn))
		return { merge(head->lex, mod, '>', head, noun_adjective, no_right) };
	return noun_det(mod, head);
}

RuleOutput head_prep(const Head& head, const Mod& mod)
{
	if (mod->is(tag::prep))
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
	if (mod->is(tag::part))
	{
		const auto result = merge(head->lex, head, '<', mod, noun_adjective, no_right);

		if (!mod->is(tag::part))
			result->errors.emplace("verb right-modifying noun must be a participle");
		else
		{
			if (mod->is(tag::past) && mod->hasBranch('+'))
				result->errors.emplace("past participle modifying noun can't have an object");
			if (mod->is(tag::pres) && mod->hasBranch(':'))
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
	if (mod->is(tag::nom))
	{
		const auto result = merge(head->lex, mod, ':', head, no_left, no_right);

		if (!mod->lex->matches<Rel::spec>(head->lex))
			result->errors.emplace("verb-subject disagreement");

		return { result };
	}
	return {};
}
RuleOutput head_comp(const Head& head, const Mod& mod)
{
	if (mod->is(tag::akk) || mod->is(tag::verb) || mod->is(tag::adn))
	{
		const auto result = merge(head->lex, head, '+', mod, head->left_rule, no_right);

		if (!mod->lex->matches<Rel::comp>(head->lex))
			result->errors.emplace("verb-object disagreement");

		if (mod->is(tag::verb) && mod->hasBranch(':'))
			result->errors.emplace("verbal object cannot have a subject");

		return { result };
	}
	return {};
}

RuleOutput verb_comp(const Head& head, const Mod& mod)
{
	auto result = head_comp(head, mod);
	if (result.empty())
		result = head_prep(head, mod);
	return result;
}
RuleOutput verb_bicomp(const Head& head, const Mod& mod)
{
	auto result = verb_comp(head, mod);
	if (mod->is(tag::akk))
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
	if (mod->is(tag::nom))
	{
		const auto match = merge(head->lex, head, ':', mod, no_left, verb_bicomp);
		
		if (!mod->lex->matches<Rel::spec>(head->lex))
			match->errors.emplace("noun-verb number/person disagreement");

		result.emplace_back(match);
	}
	return result;
}


Word::Word(Lexeme::ptr lexeme) : Phrase{ 1, move(lexeme),{} }
{
	if (lex->is(tag::nom) || lex->is(tag::akk))
	{
		left_rule = noun_adjective;
		right_rule = noun_rmod;
	}
	else if (lex->is(tag::verb))
	{
		left_rule = lex->is(tag::fin) ? verb_spec : no_left;
		right_rule = lex->is(tag::free) ? verb_rspec : verb_bicomp;
	}
	else if (lex->is(tag::adn))
	{
		left_rule = ad_adad;
	}
	else if (lex->is(tag::prep))
	{
		left_rule = no_left;
		right_rule = verb_comp;
	}
}
