# grammatical
## Proof-reader project

The idea is that for a proof-reader you want a normative grammar that can tell you what you've done wrong. The current grammar is based on the description of English grammar found at http://learnenglish.britishcouncil.org/en/. It is super-incomplete, and currently mainly support basic noun phrases, (di)transitive sentences, basic modal and auxillary verbs plus modal question sentences and some simple preposition and adverb constructions.

### Implementation
Non-probabilistic chart parser-based hybrid feature grammar with theta-role/semantics-based early filtering
 - Keeps working with partial parses even when they contain syntactic and semantic errors (and eventually spelling errors)
 - Agenda is prioritized by error count, so that all zero-error partial parses are done examined before all single-error parses etc
 - Keeps going until the agenda is empty or a full parse has been found and there are no more items on the agenda with the same error count as that parse
 
 Typical output:
 ```
 1: [have +[arriv -ed]]
2: [they: [will +come]]
3: [the: book]
4: [that: [English> [teach -er]]]
5: [a: wish]
6: [my: [latest> idea]]
7: [[[comput -er] -s]: are] [very> expensive]
8: [[do :you] +[sell +[old> [book -s]]]]
9: [we: [ate +[a: [lot <[of +food]]]]]
10: [we: [bought +[some: [new> furniture]]]]
11: [that: [is +[useful> information]]]
12: [he: [[gave *me] +[some: [useful> advice]]]]
13: [they: [[gave *us] +[a: [lot <[of +information]]]]]
14: [[let *me] +[[give *you] +[some: advice]]]
15: [[let *me] +[[give *you] +[a: [piece <[of +advice]]]]]
16: [that: [is +[a: [useful> [piece <[of +equipment]]]]]]
17: [she <[had :[six> [separate> [[item -s] <[of +luggage]]]]]]
17: [she: [had +[six> [separate> [[item -s] <[of +luggage]]]]]]
18: [everybody: [is +[watch -ing]]]
19: [[is :everybody] +[watch -ing]]
20: [they: [had +[[work -ed] +hard]]]
21: [[had :they] +[[work -ed] +hard]]
22: [he: [has +[[finish -ed] +work]]]
22: [he: [has +[[finish -ed] +work]]]
23: [[has :he] +[[finish -ed] +work]]
23: [[has :he] +[[finish -ed] +work]]
24: [everybody: [had +[been +[[work -ing] +hard]]]]
25: [[had :everybody] +[been +[[work -ing] +hard]]]
26: [[will :they] +come]
27: [he: [might +come]]
28: [[might :he] +come]
29: [they: [[will +[have +[arriv -ed]]] <[by +now]]]
30: [[[will :they] +[have +[arriv -ed]]] <[by +now]]
31: [she: [would +[have +[been +[listen -ing]]]]]
32: [[would :she] +[have +[been +[listen -ing]]]]
33: [[the: work]: [[will +[be +[finish -ed]]] <soon]]
33: [[the: work]: [will +[be +[[finish -ed] <soon]]]]
34: [[will :[the: work]] +[be +[[finish -ed] <soon]]]
34: [[[will :[the: work]] +[be +[finish -ed]]] <soon]
35: [they: [might +[have +[been +[[invit -ed] <[to +[the: party]]]]]]]
36: [[might :they] +[have +[been +[[invit -ed] <[to +[the: party]]]]]]
37: [they: eat] garf
  * unknown word garf
38: [they: [[give *me] +[book -s]]]
39: [he: [[give *me] +[book -s]]]
  * verb-subject disagreement
 ```
 Unmarked branches are phrase heads
 
 `:` is specification (subject for verbs, determiner for nouns, etc)
 
 `+` is complement/direct object
 
 `*` is indirect object
 
 `>` and `<` are modification from left and right respectively
 
 `-` is morphologic modification
 
 Note that sentence 7 does not have a full parse, since the COP+ADJ construction is not reckognized properly yet.
