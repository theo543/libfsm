/*
 * Automatically generated from the files:
 *	src/libre/dialect/native/parser.sid
 * and
 *	src/libre/parser.act
 * by:
 *	sid
 */

/* BEGINNING OF HEADER */

#line 142 "src/libre/parser.act"


	#include <assert.h>
	#include <limits.h>
	#include <string.h>
	#include <stdint.h>
	#include <stdlib.h>
	#include <stdio.h>
	#include <errno.h>
	#include <ctype.h>

	#include <re/re.h>

	#include "libre/class.h"
	#include "libre/class_lookup.h"
	#include "libre/ast.h"

	#ifndef DIALECT
	#error DIALECT required
	#endif

	#define PASTE(a, b) a ## b
	#define CAT(a, b)   PASTE(a, b)

	#define LX_PREFIX CAT(lx_, DIALECT)

	#define LX_TOKEN   CAT(LX_PREFIX, _token)
	#define LX_STATE   CAT(LX_PREFIX, _lx)
	#define LX_NEXT    CAT(LX_PREFIX, _next)
	#define LX_INIT    CAT(LX_PREFIX, _init)

	#define DIALECT_PARSE  CAT(parse_re_, DIALECT)
	#define DIALECT_CLASS  CAT(re_class_, DIALECT)

	/* XXX: get rid of this; use same %entry% for all grammars */
	#define DIALECT_ENTRY CAT(p_re__, DIALECT)

	#define TOK_CLASS__alnum  TOK_CLASS_ALNUM
	#define TOK_CLASS__alpha  TOK_CLASS_ALPHA
	#define TOK_CLASS__any    TOK_CLASS_ANY
	#define TOK_CLASS__ascii  TOK_CLASS_ASCII
	#define TOK_CLASS__blank  TOK_CLASS_BLANK
	#define TOK_CLASS__cntrl  TOK_CLASS_CNTRL
	#define TOK_CLASS__digit  TOK_CLASS_DIGIT
	#define TOK_CLASS__graph  TOK_CLASS_GRAPH
	#define TOK_CLASS__lower  TOK_CLASS_LOWER
	#define TOK_CLASS__print  TOK_CLASS_PRINT
	#define TOK_CLASS__punct  TOK_CLASS_PUNCT
	#define TOK_CLASS__space  TOK_CLASS_SPACE
	#define TOK_CLASS__spchr  TOK_CLASS_SPCHR
	#define TOK_CLASS__upper  TOK_CLASS_UPPER
	#define TOK_CLASS__word   TOK_CLASS_WORD
	#define TOK_CLASS__xdigit TOK_CLASS_XDIGIT

	#define TOK_CLASS__nspace  TOK_CLASS_NSPACE
	#define TOK_CLASS__ndigit  TOK_CLASS_NDIGIT

	/* This is a hack to work around the AST files not being able to include lexer.h. */
	#define AST_POS_OF_LX_POS(AST_POS, LX_POS) \
	    do {                                   \
	        AST_POS.line = LX_POS.line;        \
	        AST_POS.col = LX_POS.col;          \
	        AST_POS.byte = LX_POS.byte;        \
	    } while (0)

	#include "parser.h"
	#include "lexer.h"

	#include "../comp.h"
	#include "../../class.h"

	struct flags {
		enum re_flags flags;
		struct flags *parent;
	};

	typedef char     t_char;
	typedef unsigned t_unsigned;
	typedef unsigned t_pred; /* TODO */

	typedef struct lx_pos t_pos;
	typedef enum re_flags t_re__flags;
	typedef const struct class * t_ast__class__id;
	typedef struct ast_count t_ast__count;
	typedef struct ast_endpoint t_endpoint;

	struct act_state {
		enum LX_TOKEN lex_tok;
		enum LX_TOKEN lex_tok_save;
		int overlap; /* permit overlap in groups */

		/*
		 * Lexical position stored for syntax errors.
		 */
		struct re_pos synstart;
		struct re_pos synend;

		/*
		 * Lexical positions stored for errors which describe multiple tokens.
		 * We're able to store these without needing a stack, because these are
		 * non-recursive productions.
		 */
		struct re_pos groupstart; struct re_pos groupend;
		struct re_pos rangestart; struct re_pos rangeend;
		struct re_pos countstart; struct re_pos countend;
	};

	struct lex_state {
		struct LX_STATE lx;
		struct lx_dynbuf buf; /* XXX: unneccessary since we're lexing from a string */

		re_getchar_fun *f;
		void *opaque;

		/* TODO: use lx's generated conveniences for the pattern buffer */
		char a[512];
		char *p;
	};

	#define CURRENT_TERMINAL (act_state->lex_tok)
	#define ERROR_TERMINAL   (TOK_ERROR)
	#define ADVANCE_LEXER    do { mark(&act_state->synstart, &lex_state->lx.start); \
	                              mark(&act_state->synend,   &lex_state->lx.end);   \
	                              act_state->lex_tok = LX_NEXT(&lex_state->lx); \
		} while (0)
	#define SAVE_LEXER(tok)  do { act_state->lex_tok_save = act_state->lex_tok; \
	                              act_state->lex_tok = tok;                     } while (0)
	#define RESTORE_LEXER    do { act_state->lex_tok = act_state->lex_tok_save; } while (0)

	static void
	mark(struct re_pos *r, const struct lx_pos *pos)
	{
		assert(r != NULL);
		assert(pos != NULL);

		r->byte = pos->byte;
	}

	/* TODO: centralise perhaps */
	static void
	snprintdots(char *s, size_t sz, const char *msg)
	{
		size_t n;

		assert(s != NULL);
		assert(sz > 3);
		assert(msg != NULL);

		n = sprintf(s, "%.*s", (int) sz - 3 - 1, msg);
		if (n == sz - 3 - 1) {
			strcpy(s + sz, "...");
		}
	}

	/* TODO: centralise */
	/* XXX: escaping really depends on dialect */
	static const char *
	escchar(char *s, size_t sz, int c)
	{
		size_t i;

		const struct {
			int c;
			const char *s;
		} a[] = {
			{ '\\', "\\\\" },

			{ '^',  "\\^"  },
			{ '-',  "\\-"  },
			{ ']',  "\\]"  },
			{ '[',  "\\["  },

			{ '\f', "\\f"  },
			{ '\n', "\\n"  },
			{ '\r', "\\r"  },
			{ '\t', "\\t"  },
			{ '\v', "\\v"  }
		};

		assert(s != NULL);
		assert(sz >= 5);

		(void) sz;

		for (i = 0; i < sizeof a / sizeof *a; i++) {
			if (a[i].c == c) {
				return a[i].s;
			}
		}

		if (!isprint((unsigned char) c)) {
			sprintf(s, "\\x%02X", (unsigned char) c);
			return s;
		}

		sprintf(s, "%c", c);
		return s;
	}

#line 213 "src/libre/dialect/native/parser.c"


#ifndef ERROR_TERMINAL
#error "-s no-numeric-terminals given and ERROR_TERMINAL is not defined"
#endif

/* BEGINNING OF FUNCTION DECLARATIONS */

static void p_expr_C_Ccharacter_Hclass_C_Cclass_Hhead(flags, lex_state, act_state, err, t_ast__expr *);
static void p_expr_C_Ccharacter_Hclass_C_Clist_Hof_Hclass_Hterms(flags, lex_state, act_state, err, t_ast__expr);
static void p_expr_C_Cliteral(flags, lex_state, act_state, err, t_ast__expr *);
static void p_expr_C_Ccharacter_Hclass_C_Cclass_Hterm(flags, lex_state, act_state, err, t_ast__expr *);
static void p_expr_C_Ccharacter_Hclass(flags, lex_state, act_state, err, t_ast__expr *);
static void p_expr(flags, lex_state, act_state, err, t_ast__expr *);
extern void p_re__native(flags, lex_state, act_state, err, t_ast__expr *);
static void p_expr_C_Clist_Hof_Hatoms(flags, lex_state, act_state, err, t_ast__expr);
static void p_213(flags, lex_state, act_state, err, t_char *, t_pos *, t_ast__expr *);
static void p_216(flags, lex_state, act_state, err, t_ast__expr *, t_pos *, t_unsigned *, t_ast__expr *);
static void p_expr_C_Clist_Hof_Halts(flags, lex_state, act_state, err, t_ast__expr);
static void p_expr_C_Catom(flags, lex_state, act_state, err, t_ast__expr *);
static void p_expr_C_Ccharacter_Hclass_C_Cclass_Hnamed(flags, lex_state, act_state, err, t_ast__expr *);
static void p_expr_C_Calt(flags, lex_state, act_state, err, t_ast__expr *);

/* BEGINNING OF STATIC VARIABLES */


/* BEGINNING OF FUNCTION DEFINITIONS */

static void
p_expr_C_Ccharacter_Hclass_C_Cclass_Hhead(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZIclass)
{
	switch (CURRENT_TERMINAL) {
	case (TOK_INVERT):
		{
			t_char ZI108;

			/* BEGINNING OF EXTRACT: INVERT */
			{
#line 237 "src/libre/parser.act"

		ZI108 = '^';
	
#line 256 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: INVERT */
			ADVANCE_LEXER;
			/* BEGINNING OF ACTION: ast-make-invert */
			{
#line 781 "src/libre/parser.act"

		struct ast_expr *any;

		/*
		 * Here we're using an AST_EXPR_SUBTRACT node to implement inversion.
		 *
		 * Inversion is not quite equivalent to fsm_complement, because the
		 * complement of a class FSM (which is structured always left-to-right
		 * with no repetition) will effectively create .* anchors at the start
		 * and end (because the class didn't contain those).
		 *
		 * If those were added to a class beforehand, then fsm_complement()
		 * would suffice here (which would be attractive because it's much
		 * cheaper than constructing two FSM and subtracting them). However
		 * that would entail the assumption of having no transitions back to
		 * the start and end nodes, or introducing guard epsilons.
		 *
		 * I'd like to avoid those guards. And, unfortunately we'd still need
		 * to construct an intermediate FSM for fsm_complement() in any case.
		 * (And less typical, but we do still need AST_EXPR_SUBTRACT for sake
		 * of the SQL 2003 dialect anyway).
		 *
		 * So that idea doesn't satisfy my goal of avoiding intermediate FSM,
		 * and so I'm going with the simpler thing here until we come across
		 * a better idea.
		 */

		any = ast_make_expr_named(&class_any);
		if (any == NULL) {
			goto ZL1;
		}

		(*ZIclass) = ast_make_expr_subtract(any, (*ZIclass));
		if ((*ZIclass) == NULL) {
			goto ZL1;
		}
	
#line 300 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-make-invert */
		}
		break;
	case (ERROR_TERMINAL):
		return;
	default:
		break;
	}
	return;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
}

static void
p_expr_C_Ccharacter_Hclass_C_Clist_Hof_Hclass_Hterms(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr ZIclass)
{
	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
ZL2_expr_C_Ccharacter_Hclass_C_Clist_Hof_Hclass_Hterms:;
	{
		/* BEGINNING OF INLINE: 152 */
		{
			{
				t_ast__expr ZInode;

				p_expr_C_Ccharacter_Hclass_C_Cclass_Hterm (flags, lex_state, act_state, err, &ZInode);
				if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
					RESTORE_LEXER;
					goto ZL4;
				}
				/* BEGINNING OF ACTION: ast-add-alt */
				{
#line 837 "src/libre/parser.act"

		if (!ast_add_expr_alt((ZIclass), (ZInode))) {
			goto ZL4;
		}
	
#line 342 "src/libre/dialect/native/parser.c"
				}
				/* END OF ACTION: ast-add-alt */
			}
			goto ZL3;
		ZL4:;
			{
				/* BEGINNING OF ACTION: err-expected-term */
				{
#line 469 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXTERM;
		}
		goto ZL1;
	
#line 358 "src/libre/dialect/native/parser.c"
				}
				/* END OF ACTION: err-expected-term */
			}
		ZL3:;
		}
		/* END OF INLINE: 152 */
		/* BEGINNING OF INLINE: 153 */
		{
			switch (CURRENT_TERMINAL) {
			case (TOK_NAMED__CLASS): case (TOK_ESC): case (TOK_OCT): case (TOK_HEX):
			case (TOK_CHAR):
				{
					/* BEGINNING OF INLINE: expr::character-class::list-of-class-terms */
					goto ZL2_expr_C_Ccharacter_Hclass_C_Clist_Hof_Hclass_Hterms;
					/* END OF INLINE: expr::character-class::list-of-class-terms */
				}
				/* UNREACHED */
			default:
				break;
			}
		}
		/* END OF INLINE: 153 */
	}
	return;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
}

static void
p_expr_C_Cliteral(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
	{
		t_char ZIc;

		/* BEGINNING OF INLINE: 93 */
		{
			switch (CURRENT_TERMINAL) {
			case (TOK_CHAR):
				{
					t_pos ZI101;
					t_pos ZI102;

					/* BEGINNING OF EXTRACT: CHAR */
					{
#line 414 "src/libre/parser.act"

		/* the first byte may be '\x00' */
		assert(lex_state->buf.a[1] == '\0');

		ZI101 = lex_state->lx.start;
		ZI102   = lex_state->lx.end;

		ZIc = lex_state->buf.a[0];
	
#line 419 "src/libre/dialect/native/parser.c"
					}
					/* END OF EXTRACT: CHAR */
					ADVANCE_LEXER;
				}
				break;
			case (TOK_ESC):
				{
					t_pos ZI95;
					t_pos ZI96;

					/* BEGINNING OF EXTRACT: ESC */
					{
#line 287 "src/libre/parser.act"

		assert(lex_state->buf.a[0] == '\\');
		assert(lex_state->buf.a[1] != '\0');
		assert(lex_state->buf.a[2] == '\0');

		ZIc = lex_state->buf.a[1];

		switch (ZIc) {
		case 'a': ZIc = '\a'; break;
		case 'f': ZIc = '\f'; break;
		case 'n': ZIc = '\n'; break;
		case 'r': ZIc = '\r'; break;
		case 't': ZIc = '\t'; break;
		case 'v': ZIc = '\v'; break;
		default:             break;
		}

		ZI95 = lex_state->lx.start;
		ZI96   = lex_state->lx.end;
	
#line 453 "src/libre/dialect/native/parser.c"
					}
					/* END OF EXTRACT: ESC */
					ADVANCE_LEXER;
				}
				break;
			case (TOK_HEX):
				{
					t_pos ZI99;
					t_pos ZI100;

					/* BEGINNING OF EXTRACT: HEX */
					{
#line 378 "src/libre/parser.act"

		unsigned long u;
		char *s, *e;
		int brace = 0;

		assert(0 == strncmp(lex_state->buf.a, "\\x", 2));
		assert(strlen(lex_state->buf.a) >= 3);

		ZI99 = lex_state->lx.start;
		ZI100   = lex_state->lx.end;

		errno = 0;

		s = lex_state->buf.a + 2;

		if (*s == '{') {
			s++;
			brace = 1;
		}

		u = strtoul(s, &e, 16);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UCHAR_MAX) {
			err->e = RE_EHEXRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL1;
		}

		if (brace && *e == '}') {
			e++;
		}

		if ((u == ULONG_MAX && errno != 0) || (*e != '\0')) {
			err->e = RE_EXESC;
			goto ZL1;
		}

		ZIc = (char) (unsigned char) u;
	
#line 506 "src/libre/dialect/native/parser.c"
					}
					/* END OF EXTRACT: HEX */
					ADVANCE_LEXER;
				}
				break;
			case (TOK_OCT):
				{
					t_pos ZI97;
					t_pos ZI98;

					/* BEGINNING OF EXTRACT: OCT */
					{
#line 338 "src/libre/parser.act"

		unsigned long u;
		char *s, *e;
		int brace = 0;

		assert(0 == strncmp(lex_state->buf.a, "\\", 1));
		assert(strlen(lex_state->buf.a) >= 2);

		ZI97 = lex_state->lx.start;
		ZI98   = lex_state->lx.end;

		errno = 0;

		s = lex_state->buf.a + 1;

		if (s[0] == 'o' && s[1] == '{') {
			s += 2;
			brace = 1;
		}

		u = strtoul(s, &e, 8);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UCHAR_MAX) {
			err->e = RE_EOCTRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL1;
		}

		if (brace && *e == '}') {
			e++;
		}

		if ((u == ULONG_MAX && errno != 0) || *e != '\0') {
			err->e = RE_EXESC;
			goto ZL1;
		}

		ZIc = (char) (unsigned char) u;
	
#line 559 "src/libre/dialect/native/parser.c"
					}
					/* END OF EXTRACT: OCT */
					ADVANCE_LEXER;
				}
				break;
			default:
				goto ZL1;
			}
		}
		/* END OF INLINE: 93 */
		/* BEGINNING OF ACTION: ast-make-literal */
		{
#line 678 "src/libre/parser.act"

		(ZInode) = ast_make_expr_literal((ZIc));
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 579 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: ast-make-literal */
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_expr_C_Ccharacter_Hclass_C_Cclass_Hterm(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	switch (CURRENT_TERMINAL) {
	case (TOK_CHAR):
		{
			t_char ZI210;
			t_pos ZI211;
			t_pos ZI212;

			/* BEGINNING OF EXTRACT: CHAR */
			{
#line 414 "src/libre/parser.act"

		/* the first byte may be '\x00' */
		assert(lex_state->buf.a[1] == '\0');

		ZI211 = lex_state->lx.start;
		ZI212   = lex_state->lx.end;

		ZI210 = lex_state->buf.a[0];
	
#line 615 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: CHAR */
			ADVANCE_LEXER;
			p_213 (flags, lex_state, act_state, err, &ZI210, &ZI211, &ZInode);
			if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
				RESTORE_LEXER;
				goto ZL1;
			}
		}
		break;
	case (TOK_ESC):
		{
			t_char ZI198;
			t_pos ZI199;
			t_pos ZI200;

			/* BEGINNING OF EXTRACT: ESC */
			{
#line 287 "src/libre/parser.act"

		assert(lex_state->buf.a[0] == '\\');
		assert(lex_state->buf.a[1] != '\0');
		assert(lex_state->buf.a[2] == '\0');

		ZI198 = lex_state->buf.a[1];

		switch (ZI198) {
		case 'a': ZI198 = '\a'; break;
		case 'f': ZI198 = '\f'; break;
		case 'n': ZI198 = '\n'; break;
		case 'r': ZI198 = '\r'; break;
		case 't': ZI198 = '\t'; break;
		case 'v': ZI198 = '\v'; break;
		default:             break;
		}

		ZI199 = lex_state->lx.start;
		ZI200   = lex_state->lx.end;
	
#line 655 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: ESC */
			ADVANCE_LEXER;
			p_213 (flags, lex_state, act_state, err, &ZI198, &ZI199, &ZInode);
			if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
				RESTORE_LEXER;
				goto ZL1;
			}
		}
		break;
	case (TOK_HEX):
		{
			t_char ZI206;
			t_pos ZI207;
			t_pos ZI208;

			/* BEGINNING OF EXTRACT: HEX */
			{
#line 378 "src/libre/parser.act"

		unsigned long u;
		char *s, *e;
		int brace = 0;

		assert(0 == strncmp(lex_state->buf.a, "\\x", 2));
		assert(strlen(lex_state->buf.a) >= 3);

		ZI207 = lex_state->lx.start;
		ZI208   = lex_state->lx.end;

		errno = 0;

		s = lex_state->buf.a + 2;

		if (*s == '{') {
			s++;
			brace = 1;
		}

		u = strtoul(s, &e, 16);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UCHAR_MAX) {
			err->e = RE_EHEXRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL1;
		}

		if (brace && *e == '}') {
			e++;
		}

		if ((u == ULONG_MAX && errno != 0) || (*e != '\0')) {
			err->e = RE_EXESC;
			goto ZL1;
		}

		ZI206 = (char) (unsigned char) u;
	
#line 714 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: HEX */
			ADVANCE_LEXER;
			p_213 (flags, lex_state, act_state, err, &ZI206, &ZI207, &ZInode);
			if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
				RESTORE_LEXER;
				goto ZL1;
			}
		}
		break;
	case (TOK_OCT):
		{
			t_char ZI202;
			t_pos ZI203;
			t_pos ZI204;

			/* BEGINNING OF EXTRACT: OCT */
			{
#line 338 "src/libre/parser.act"

		unsigned long u;
		char *s, *e;
		int brace = 0;

		assert(0 == strncmp(lex_state->buf.a, "\\", 1));
		assert(strlen(lex_state->buf.a) >= 2);

		ZI203 = lex_state->lx.start;
		ZI204   = lex_state->lx.end;

		errno = 0;

		s = lex_state->buf.a + 1;

		if (s[0] == 'o' && s[1] == '{') {
			s += 2;
			brace = 1;
		}

		u = strtoul(s, &e, 8);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UCHAR_MAX) {
			err->e = RE_EOCTRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL1;
		}

		if (brace && *e == '}') {
			e++;
		}

		if ((u == ULONG_MAX && errno != 0) || *e != '\0') {
			err->e = RE_EXESC;
			goto ZL1;
		}

		ZI202 = (char) (unsigned char) u;
	
#line 773 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: OCT */
			ADVANCE_LEXER;
			p_213 (flags, lex_state, act_state, err, &ZI202, &ZI203, &ZInode);
			if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
				RESTORE_LEXER;
				goto ZL1;
			}
		}
		break;
	case (TOK_NAMED__CLASS):
		{
			p_expr_C_Ccharacter_Hclass_C_Cclass_Hnamed (flags, lex_state, act_state, err, &ZInode);
			if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
				RESTORE_LEXER;
				goto ZL1;
			}
		}
		break;
	case (ERROR_TERMINAL):
		return;
	default:
		goto ZL1;
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_expr_C_Ccharacter_Hclass(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
	{
		t_pos ZIstart;
		t_pos ZI154;
		t_ast__expr ZItmp;
		t_pos ZIend;

		switch (CURRENT_TERMINAL) {
		case (TOK_OPENGROUP):
			/* BEGINNING OF EXTRACT: OPENGROUP */
			{
#line 247 "src/libre/parser.act"

		ZIstart = lex_state->lx.start;
		ZI154   = lex_state->lx.end;
	
#line 829 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: OPENGROUP */
			break;
		default:
			goto ZL1;
		}
		ADVANCE_LEXER;
		/* BEGINNING OF ACTION: ast-make-alt */
		{
#line 671 "src/libre/parser.act"

		(ZInode) = ast_make_expr_alt();
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 846 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: ast-make-alt */
		ZItmp = ZInode;
		p_expr_C_Ccharacter_Hclass_C_Cclass_Hhead (flags, lex_state, act_state, err, &ZInode);
		p_expr_C_Ccharacter_Hclass_C_Clist_Hof_Hclass_Hterms (flags, lex_state, act_state, err, ZItmp);
		/* BEGINNING OF INLINE: 157 */
		{
			if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
				RESTORE_LEXER;
				goto ZL1;
			}
			{
				t_char ZI158;
				t_pos ZI159;

				switch (CURRENT_TERMINAL) {
				case (TOK_CLOSEGROUP):
					/* BEGINNING OF EXTRACT: CLOSEGROUP */
					{
#line 267 "src/libre/parser.act"

		ZI158 = ']';
		ZI159 = lex_state->lx.start;
		ZIend   = lex_state->lx.end;
	
#line 872 "src/libre/dialect/native/parser.c"
					}
					/* END OF EXTRACT: CLOSEGROUP */
					break;
				default:
					goto ZL3;
				}
				ADVANCE_LEXER;
				/* BEGINNING OF ACTION: mark-group */
				{
#line 543 "src/libre/parser.act"

		mark(&act_state->groupstart, &(ZIstart));
		mark(&act_state->groupend,   &(ZIend));
	
#line 887 "src/libre/dialect/native/parser.c"
				}
				/* END OF ACTION: mark-group */
			}
			goto ZL2;
		ZL3:;
			{
				/* BEGINNING OF ACTION: err-expected-closegroup */
				{
#line 504 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXCLOSEGROUP;
		}
		goto ZL1;
	
#line 903 "src/libre/dialect/native/parser.c"
				}
				/* END OF ACTION: err-expected-closegroup */
				ZIend = ZIstart;
			}
		ZL2:;
		}
		/* END OF INLINE: 157 */
		/* BEGINNING OF ACTION: mark-expr */
		{
#line 560 "src/libre/parser.act"

		struct ast_pos ast_start, ast_end;

		AST_POS_OF_LX_POS(ast_start, (ZIstart));
		AST_POS_OF_LX_POS(ast_end, (ZIend));

		mark(&act_state->groupstart, &(ZIstart));
		mark(&act_state->groupend,   &(ZIend));

/* TODO: reinstate this, applies to an expr node in general
		(ZItmp)->u.class.start = ast_start;
		(ZItmp)->u.class.end   = ast_end;
*/
	
#line 928 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: mark-expr */
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_expr(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	switch (CURRENT_TERMINAL) {
	case (TOK_ANY): case (TOK_START): case (TOK_END): case (TOK_OPENSUB):
	case (TOK_OPENGROUP): case (TOK_ESC): case (TOK_OCT): case (TOK_HEX):
	case (TOK_CHAR):
		{
			/* BEGINNING OF ACTION: ast-make-alt */
			{
#line 671 "src/libre/parser.act"

		(ZInode) = ast_make_expr_alt();
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 959 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-make-alt */
			p_expr_C_Clist_Hof_Halts (flags, lex_state, act_state, err, ZInode);
			if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
				RESTORE_LEXER;
				goto ZL1;
			}
		}
		break;
	default:
		{
			/* BEGINNING OF ACTION: ast-make-empty */
			{
#line 657 "src/libre/parser.act"

		(ZInode) = ast_make_expr_empty();
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 980 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-make-empty */
		}
		break;
	case (ERROR_TERMINAL):
		return;
	}
	goto ZL0;
ZL1:;
	{
		/* BEGINNING OF ACTION: err-expected-alts */
		{
#line 490 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXALTS;
		}
		goto ZL2;
	
#line 1000 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: err-expected-alts */
		/* BEGINNING OF ACTION: ast-make-empty */
		{
#line 657 "src/libre/parser.act"

		(ZInode) = ast_make_expr_empty();
		if ((ZInode) == NULL) {
			goto ZL2;
		}
	
#line 1012 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: ast-make-empty */
	}
	goto ZL0;
ZL2:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

void
p_re__native(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
	{
		/* BEGINNING OF INLINE: 186 */
		{
			{
				p_expr (flags, lex_state, act_state, err, &ZInode);
				if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
					RESTORE_LEXER;
					goto ZL1;
				}
			}
		}
		/* END OF INLINE: 186 */
		/* BEGINNING OF INLINE: 187 */
		{
			{
				switch (CURRENT_TERMINAL) {
				case (TOK_EOF):
					break;
				default:
					goto ZL4;
				}
				ADVANCE_LEXER;
			}
			goto ZL3;
		ZL4:;
			{
				/* BEGINNING OF ACTION: err-expected-eof */
				{
#line 532 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXEOF;
		}
		goto ZL1;
	
#line 1067 "src/libre/dialect/native/parser.c"
				}
				/* END OF ACTION: err-expected-eof */
			}
		ZL3:;
		}
		/* END OF INLINE: 187 */
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_expr_C_Clist_Hof_Hatoms(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr ZIcat)
{
	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
ZL2_expr_C_Clist_Hof_Hatoms:;
	{
		t_ast__expr ZIa;

		p_expr_C_Catom (flags, lex_state, act_state, err, &ZIa);
		if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
			RESTORE_LEXER;
			goto ZL1;
		}
		/* BEGINNING OF ACTION: ast-add-concat */
		{
#line 831 "src/libre/parser.act"

		if (!ast_add_expr_concat((ZIcat), (ZIa))) {
			goto ZL1;
		}
	
#line 1106 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: ast-add-concat */
		/* BEGINNING OF INLINE: 178 */
		{
			switch (CURRENT_TERMINAL) {
			case (TOK_ANY): case (TOK_START): case (TOK_END): case (TOK_OPENSUB):
			case (TOK_OPENGROUP): case (TOK_ESC): case (TOK_OCT): case (TOK_HEX):
			case (TOK_CHAR):
				{
					/* BEGINNING OF INLINE: expr::list-of-atoms */
					goto ZL2_expr_C_Clist_Hof_Hatoms;
					/* END OF INLINE: expr::list-of-atoms */
				}
				/* UNREACHED */
			default:
				break;
			}
		}
		/* END OF INLINE: 178 */
	}
	return;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
}

static void
p_213(flags flags, lex_state lex_state, act_state act_state, err err, t_char *ZI210, t_pos *ZI211, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	switch (CURRENT_TERMINAL) {
	default:
		{
			/* BEGINNING OF ACTION: ast-make-literal */
			{
#line 678 "src/libre/parser.act"

		(ZInode) = ast_make_expr_literal((*ZI210));
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 1150 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-make-literal */
		}
		break;
	case (TOK_RANGE):
		{
			t_endpoint ZIa;
			t_char ZIcz;
			t_pos ZIend;
			t_endpoint ZIz;

			/* BEGINNING OF ACTION: ast-range-endpoint-literal */
			{
#line 626 "src/libre/parser.act"

		(ZIa).type = AST_ENDPOINT_LITERAL;
		(ZIa).u.literal.c = (*ZI210);
	
#line 1169 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-range-endpoint-literal */
			/* BEGINNING OF INLINE: 130 */
			{
				{
					t_char ZI131;
					t_pos ZI132;
					t_pos ZI133;

					switch (CURRENT_TERMINAL) {
					case (TOK_RANGE):
						/* BEGINNING OF EXTRACT: RANGE */
						{
#line 241 "src/libre/parser.act"

		ZI131 = '-';
		ZI132 = lex_state->lx.start;
		ZI133   = lex_state->lx.end;
	
#line 1189 "src/libre/dialect/native/parser.c"
						}
						/* END OF EXTRACT: RANGE */
						break;
					default:
						goto ZL3;
					}
					ADVANCE_LEXER;
				}
				goto ZL2;
			ZL3:;
				{
					/* BEGINNING OF ACTION: err-expected-range */
					{
#line 497 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXRANGE;
		}
		goto ZL1;
	
#line 1210 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: err-expected-range */
				}
			ZL2:;
			}
			/* END OF INLINE: 130 */
			/* BEGINNING OF INLINE: 134 */
			{
				switch (CURRENT_TERMINAL) {
				case (TOK_CHAR):
					{
						t_pos ZI140;

						/* BEGINNING OF EXTRACT: CHAR */
						{
#line 414 "src/libre/parser.act"

		/* the first byte may be '\x00' */
		assert(lex_state->buf.a[1] == '\0');

		ZI140 = lex_state->lx.start;
		ZIend   = lex_state->lx.end;

		ZIcz = lex_state->buf.a[0];
	
#line 1236 "src/libre/dialect/native/parser.c"
						}
						/* END OF EXTRACT: CHAR */
						ADVANCE_LEXER;
					}
					break;
				case (TOK_ESC):
					{
						t_pos ZI136;

						/* BEGINNING OF EXTRACT: ESC */
						{
#line 287 "src/libre/parser.act"

		assert(lex_state->buf.a[0] == '\\');
		assert(lex_state->buf.a[1] != '\0');
		assert(lex_state->buf.a[2] == '\0');

		ZIcz = lex_state->buf.a[1];

		switch (ZIcz) {
		case 'a': ZIcz = '\a'; break;
		case 'f': ZIcz = '\f'; break;
		case 'n': ZIcz = '\n'; break;
		case 'r': ZIcz = '\r'; break;
		case 't': ZIcz = '\t'; break;
		case 'v': ZIcz = '\v'; break;
		default:             break;
		}

		ZI136 = lex_state->lx.start;
		ZIend   = lex_state->lx.end;
	
#line 1269 "src/libre/dialect/native/parser.c"
						}
						/* END OF EXTRACT: ESC */
						ADVANCE_LEXER;
					}
					break;
				case (TOK_HEX):
					{
						t_pos ZI139;

						/* BEGINNING OF EXTRACT: HEX */
						{
#line 378 "src/libre/parser.act"

		unsigned long u;
		char *s, *e;
		int brace = 0;

		assert(0 == strncmp(lex_state->buf.a, "\\x", 2));
		assert(strlen(lex_state->buf.a) >= 3);

		ZI139 = lex_state->lx.start;
		ZIend   = lex_state->lx.end;

		errno = 0;

		s = lex_state->buf.a + 2;

		if (*s == '{') {
			s++;
			brace = 1;
		}

		u = strtoul(s, &e, 16);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UCHAR_MAX) {
			err->e = RE_EHEXRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL1;
		}

		if (brace && *e == '}') {
			e++;
		}

		if ((u == ULONG_MAX && errno != 0) || (*e != '\0')) {
			err->e = RE_EXESC;
			goto ZL1;
		}

		ZIcz = (char) (unsigned char) u;
	
#line 1321 "src/libre/dialect/native/parser.c"
						}
						/* END OF EXTRACT: HEX */
						ADVANCE_LEXER;
					}
					break;
				case (TOK_OCT):
					{
						t_pos ZI138;

						/* BEGINNING OF EXTRACT: OCT */
						{
#line 338 "src/libre/parser.act"

		unsigned long u;
		char *s, *e;
		int brace = 0;

		assert(0 == strncmp(lex_state->buf.a, "\\", 1));
		assert(strlen(lex_state->buf.a) >= 2);

		ZI138 = lex_state->lx.start;
		ZIend   = lex_state->lx.end;

		errno = 0;

		s = lex_state->buf.a + 1;

		if (s[0] == 'o' && s[1] == '{') {
			s += 2;
			brace = 1;
		}

		u = strtoul(s, &e, 8);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UCHAR_MAX) {
			err->e = RE_EOCTRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL1;
		}

		if (brace && *e == '}') {
			e++;
		}

		if ((u == ULONG_MAX && errno != 0) || *e != '\0') {
			err->e = RE_EXESC;
			goto ZL1;
		}

		ZIcz = (char) (unsigned char) u;
	
#line 1373 "src/libre/dialect/native/parser.c"
						}
						/* END OF EXTRACT: OCT */
						ADVANCE_LEXER;
					}
					break;
				case (TOK_RANGE):
					{
						t_pos ZI141;

						/* BEGINNING OF EXTRACT: RANGE */
						{
#line 241 "src/libre/parser.act"

		ZIcz = '-';
		ZI141 = lex_state->lx.start;
		ZIend   = lex_state->lx.end;
	
#line 1391 "src/libre/dialect/native/parser.c"
						}
						/* END OF EXTRACT: RANGE */
						ADVANCE_LEXER;
					}
					break;
				default:
					goto ZL1;
				}
			}
			/* END OF INLINE: 134 */
			/* BEGINNING OF ACTION: ast-range-endpoint-literal */
			{
#line 626 "src/libre/parser.act"

		(ZIz).type = AST_ENDPOINT_LITERAL;
		(ZIz).u.literal.c = (ZIcz);
	
#line 1409 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-range-endpoint-literal */
			/* BEGINNING OF ACTION: mark-range */
			{
#line 548 "src/libre/parser.act"

		mark(&act_state->rangestart, &(*ZI211));
		mark(&act_state->rangeend,   &(ZIend));
	
#line 1419 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: mark-range */
			/* BEGINNING OF ACTION: ast-range-distinct */
			{
#line 638 "src/libre/parser.act"

		struct ast_pos ast_start, ast_end;

		AST_POS_OF_LX_POS(ast_start, (*ZI211));
		AST_POS_OF_LX_POS(ast_end, (ZIend));

		if ((ZIa).type != AST_ENDPOINT_LITERAL || (ZIz).type != AST_ENDPOINT_LITERAL) {
			err->e = RE_EXUNSUPPORTD;
			goto ZL1;
		}

		if ((ZIa).u.literal.c == (ZIz).u.literal.c) {
			err->e = RE_EDISTINCT;
			goto ZL1;
		}
	
#line 1441 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-range-distinct */
			/* BEGINNING OF ACTION: ast-make-range */
			{
#line 794 "src/libre/parser.act"

		struct ast_pos ast_start, ast_end;
		unsigned char lower, upper;

		AST_POS_OF_LX_POS(ast_start, (*ZI211));
		AST_POS_OF_LX_POS(ast_end, (ZIend));

		if ((ZIa).type != AST_ENDPOINT_LITERAL ||
			(ZIz).type != AST_ENDPOINT_LITERAL) {
			err->e = RE_EXUNSUPPORTD;
			goto ZL1;
		}

		lower = (ZIa).u.literal.c;
		upper = (ZIz).u.literal.c;

		if (lower > upper) {
			char a[5], b[5];
			
			assert(sizeof err->set >= 1 + sizeof a + 1 + sizeof b + 1 + 1);
			
			sprintf(err->set, "%s-%s",
				escchar(a, sizeof a, lower), escchar(b, sizeof b, upper));
			err->e = RE_ENEGRANGE;
			goto ZL1;
		}

		(ZInode) = ast_make_expr_range(&(ZIa), ast_start, &(ZIz), ast_end);
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 1479 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-make-range */
		}
		break;
	case (ERROR_TERMINAL):
		return;
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_216(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZIe, t_pos *ZI214, t_unsigned *ZIm, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	switch (CURRENT_TERMINAL) {
	case (TOK_CLOSECOUNT):
		{
			t_pos ZI170;
			t_pos ZIend;
			t_ast__count ZIc;

			/* BEGINNING OF EXTRACT: CLOSECOUNT */
			{
#line 278 "src/libre/parser.act"

		ZI170 = lex_state->lx.start;
		ZIend   = lex_state->lx.end;
	
#line 1514 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: CLOSECOUNT */
			ADVANCE_LEXER;
			/* BEGINNING OF ACTION: mark-count */
			{
#line 553 "src/libre/parser.act"

		mark(&act_state->countstart, &(*ZI214));
		mark(&act_state->countend,   &(ZIend));
	
#line 1525 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: mark-count */
			/* BEGINNING OF ACTION: atom-count */
			{
#line 608 "src/libre/parser.act"

		struct ast_pos ast_start, ast_end;

		if ((*ZIm) < (*ZIm)) {
			err->e = RE_ENEGCOUNT;
			err->m = (*ZIm);
			err->n = (*ZIm);

			mark(&act_state->countstart, &(*ZI214));
			mark(&act_state->countend,   &(ZIend));

			goto ZL1;
		}

		AST_POS_OF_LX_POS(ast_start, (*ZI214));
		AST_POS_OF_LX_POS(ast_end, (ZIend));

		(ZIc) = ast_make_count((*ZIm), &ast_start, (*ZIm), &ast_end);
	
#line 1550 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: atom-count */
			/* BEGINNING OF ACTION: ast-make-atom */
			{
#line 696 "src/libre/parser.act"

		(ZInode) = ast_make_expr_with_count((*ZIe), (ZIc));
		if ((ZInode) == NULL) {
			err->e = RE_EXEOF;
			goto ZL1;
		}
	
#line 1563 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-make-atom */
		}
		break;
	case (TOK_SEP):
		{
			t_unsigned ZIn;
			t_pos ZIend;
			t_pos ZI173;
			t_ast__count ZIc;

			ADVANCE_LEXER;
			switch (CURRENT_TERMINAL) {
			case (TOK_COUNT):
				/* BEGINNING OF EXTRACT: COUNT */
				{
#line 429 "src/libre/parser.act"

		unsigned long u;
		char *e;

		u = strtoul(lex_state->buf.a, &e, 10);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UINT_MAX) {
			err->e = RE_ECOUNTRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL1;
		}

		if ((u == ULONG_MAX && errno != 0) || *e != '\0') {
			err->e = RE_EXCOUNT;
			goto ZL1;
		}

		ZIn = (unsigned int) u;
	
#line 1600 "src/libre/dialect/native/parser.c"
				}
				/* END OF EXTRACT: COUNT */
				break;
			default:
				goto ZL1;
			}
			ADVANCE_LEXER;
			switch (CURRENT_TERMINAL) {
			case (TOK_CLOSECOUNT):
				/* BEGINNING OF EXTRACT: CLOSECOUNT */
				{
#line 278 "src/libre/parser.act"

		ZIend = lex_state->lx.start;
		ZI173   = lex_state->lx.end;
	
#line 1617 "src/libre/dialect/native/parser.c"
				}
				/* END OF EXTRACT: CLOSECOUNT */
				break;
			default:
				goto ZL1;
			}
			ADVANCE_LEXER;
			/* BEGINNING OF ACTION: mark-count */
			{
#line 553 "src/libre/parser.act"

		mark(&act_state->countstart, &(*ZI214));
		mark(&act_state->countend,   &(ZIend));
	
#line 1632 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: mark-count */
			/* BEGINNING OF ACTION: atom-count */
			{
#line 608 "src/libre/parser.act"

		struct ast_pos ast_start, ast_end;

		if ((ZIn) < (*ZIm)) {
			err->e = RE_ENEGCOUNT;
			err->m = (*ZIm);
			err->n = (ZIn);

			mark(&act_state->countstart, &(*ZI214));
			mark(&act_state->countend,   &(ZIend));

			goto ZL1;
		}

		AST_POS_OF_LX_POS(ast_start, (*ZI214));
		AST_POS_OF_LX_POS(ast_end, (ZIend));

		(ZIc) = ast_make_count((*ZIm), &ast_start, (ZIn), &ast_end);
	
#line 1657 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: atom-count */
			/* BEGINNING OF ACTION: ast-make-atom */
			{
#line 696 "src/libre/parser.act"

		(ZInode) = ast_make_expr_with_count((*ZIe), (ZIc));
		if ((ZInode) == NULL) {
			err->e = RE_EXEOF;
			goto ZL1;
		}
	
#line 1670 "src/libre/dialect/native/parser.c"
			}
			/* END OF ACTION: ast-make-atom */
		}
		break;
	case (ERROR_TERMINAL):
		return;
	default:
		goto ZL1;
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_expr_C_Clist_Hof_Halts(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr ZIalts)
{
	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
ZL2_expr_C_Clist_Hof_Halts:;
	{
		t_ast__expr ZIa;

		p_expr_C_Calt (flags, lex_state, act_state, err, &ZIa);
		if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
			RESTORE_LEXER;
			goto ZL1;
		}
		/* BEGINNING OF ACTION: ast-add-alt */
		{
#line 837 "src/libre/parser.act"

		if (!ast_add_expr_alt((ZIalts), (ZIa))) {
			goto ZL1;
		}
	
#line 1711 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: ast-add-alt */
		/* BEGINNING OF INLINE: 184 */
		{
			switch (CURRENT_TERMINAL) {
			case (TOK_ALT):
				{
					ADVANCE_LEXER;
					/* BEGINNING OF INLINE: expr::list-of-alts */
					goto ZL2_expr_C_Clist_Hof_Halts;
					/* END OF INLINE: expr::list-of-alts */
				}
				/* UNREACHED */
			default:
				break;
			}
		}
		/* END OF INLINE: 184 */
	}
	return;
ZL1:;
	{
		/* BEGINNING OF ACTION: err-expected-alts */
		{
#line 490 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXALTS;
		}
		goto ZL4;
	
#line 1743 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: err-expected-alts */
	}
	goto ZL0;
ZL4:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
}

static void
p_expr_C_Catom(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
	{
		t_ast__expr ZIe;

		/* BEGINNING OF INLINE: 163 */
		{
			switch (CURRENT_TERMINAL) {
			case (TOK_ANY):
				{
					ADVANCE_LEXER;
					/* BEGINNING OF ACTION: ast-make-any */
					{
#line 689 "src/libre/parser.act"

		(ZIe) = ast_make_expr_any();
		if ((ZIe) == NULL) {
			goto ZL1;
		}
	
#line 1780 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-any */
				}
				break;
			case (TOK_END):
				{
					ADVANCE_LEXER;
					/* BEGINNING OF ACTION: ast-make-anchor-end */
					{
#line 739 "src/libre/parser.act"

		(ZIe) = ast_make_expr_anchor(AST_ANCHOR_END);
		if ((ZIe) == NULL) {
			goto ZL1;
		}
	
#line 1797 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-anchor-end */
				}
				break;
			case (TOK_OPENSUB):
				{
					t_ast__expr ZIg;

					ADVANCE_LEXER;
					p_expr (flags, lex_state, act_state, err, &ZIg);
					if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
						RESTORE_LEXER;
						goto ZL1;
					}
					/* BEGINNING OF ACTION: ast-make-group */
					{
#line 718 "src/libre/parser.act"

		(ZIe) = ast_make_expr_group((ZIg));
		if ((ZIe) == NULL) {
			goto ZL1;
		}
	
#line 1821 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-group */
					/* BEGINNING OF INLINE: 166 */
					{
						{
							switch (CURRENT_TERMINAL) {
							case (TOK_CLOSESUB):
								break;
							default:
								goto ZL4;
							}
							ADVANCE_LEXER;
						}
						goto ZL3;
					ZL4:;
						{
							/* BEGINNING OF ACTION: err-expected-alts */
							{
#line 490 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXALTS;
		}
		goto ZL1;
	
#line 1847 "src/libre/dialect/native/parser.c"
							}
							/* END OF ACTION: err-expected-alts */
						}
					ZL3:;
					}
					/* END OF INLINE: 166 */
				}
				break;
			case (TOK_START):
				{
					ADVANCE_LEXER;
					/* BEGINNING OF ACTION: ast-make-anchor-start */
					{
#line 732 "src/libre/parser.act"

		(ZIe) = ast_make_expr_anchor(AST_ANCHOR_START);
		if ((ZIe) == NULL) {
			goto ZL1;
		}
	
#line 1868 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-anchor-start */
				}
				break;
			case (TOK_OPENGROUP):
				{
					p_expr_C_Ccharacter_Hclass (flags, lex_state, act_state, err, &ZIe);
					if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
						RESTORE_LEXER;
						goto ZL1;
					}
				}
				break;
			case (TOK_ESC): case (TOK_OCT): case (TOK_HEX): case (TOK_CHAR):
				{
					p_expr_C_Cliteral (flags, lex_state, act_state, err, &ZIe);
					if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
						RESTORE_LEXER;
						goto ZL1;
					}
				}
				break;
			default:
				goto ZL1;
			}
		}
		/* END OF INLINE: 163 */
		/* BEGINNING OF INLINE: 167 */
		{
			switch (CURRENT_TERMINAL) {
			case (TOK_OPENCOUNT):
				{
					t_pos ZI214;
					t_pos ZI215;
					t_unsigned ZIm;

					/* BEGINNING OF EXTRACT: OPENCOUNT */
					{
#line 273 "src/libre/parser.act"

		ZI214 = lex_state->lx.start;
		ZI215   = lex_state->lx.end;
	
#line 1912 "src/libre/dialect/native/parser.c"
					}
					/* END OF EXTRACT: OPENCOUNT */
					ADVANCE_LEXER;
					switch (CURRENT_TERMINAL) {
					case (TOK_COUNT):
						/* BEGINNING OF EXTRACT: COUNT */
						{
#line 429 "src/libre/parser.act"

		unsigned long u;
		char *e;

		u = strtoul(lex_state->buf.a, &e, 10);

		if ((u == ULONG_MAX && errno == ERANGE) || u > UINT_MAX) {
			err->e = RE_ECOUNTRANGE;
			snprintdots(err->esc, sizeof err->esc, lex_state->buf.a);
			goto ZL6;
		}

		if ((u == ULONG_MAX && errno != 0) || *e != '\0') {
			err->e = RE_EXCOUNT;
			goto ZL6;
		}

		ZIm = (unsigned int) u;
	
#line 1940 "src/libre/dialect/native/parser.c"
						}
						/* END OF EXTRACT: COUNT */
						break;
					default:
						goto ZL6;
					}
					ADVANCE_LEXER;
					p_216 (flags, lex_state, act_state, err, &ZIe, &ZI214, &ZIm, &ZInode);
					if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
						RESTORE_LEXER;
						goto ZL6;
					}
				}
				break;
			case (TOK_OPT):
				{
					t_ast__count ZIc;

					ADVANCE_LEXER;
					/* BEGINNING OF ACTION: atom-opt */
					{
#line 598 "src/libre/parser.act"

		(ZIc) = ast_make_count(0, NULL, 1, NULL);
	
#line 1966 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: atom-opt */
					/* BEGINNING OF ACTION: ast-make-atom */
					{
#line 696 "src/libre/parser.act"

		(ZInode) = ast_make_expr_with_count((ZIe), (ZIc));
		if ((ZInode) == NULL) {
			err->e = RE_EXEOF;
			goto ZL6;
		}
	
#line 1979 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-atom */
				}
				break;
			case (TOK_PLUS):
				{
					t_ast__count ZIc;

					ADVANCE_LEXER;
					/* BEGINNING OF ACTION: atom-plus */
					{
#line 590 "src/libre/parser.act"

		(ZIc) = ast_make_count(1, NULL, AST_COUNT_UNBOUNDED, NULL);
	
#line 1995 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: atom-plus */
					/* BEGINNING OF ACTION: ast-make-atom */
					{
#line 696 "src/libre/parser.act"

		(ZInode) = ast_make_expr_with_count((ZIe), (ZIc));
		if ((ZInode) == NULL) {
			err->e = RE_EXEOF;
			goto ZL6;
		}
	
#line 2008 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-atom */
				}
				break;
			case (TOK_STAR):
				{
					t_ast__count ZIc;

					ADVANCE_LEXER;
					/* BEGINNING OF ACTION: atom-kleene */
					{
#line 586 "src/libre/parser.act"

		(ZIc) = ast_make_count(0, NULL, AST_COUNT_UNBOUNDED, NULL);
	
#line 2024 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: atom-kleene */
					/* BEGINNING OF ACTION: ast-make-atom */
					{
#line 696 "src/libre/parser.act"

		(ZInode) = ast_make_expr_with_count((ZIe), (ZIc));
		if ((ZInode) == NULL) {
			err->e = RE_EXEOF;
			goto ZL6;
		}
	
#line 2037 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-atom */
				}
				break;
			default:
				{
					t_ast__count ZIc;

					/* BEGINNING OF ACTION: atom-one */
					{
#line 594 "src/libre/parser.act"

		(ZIc) = ast_make_count(1, NULL, 1, NULL);
	
#line 2052 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: atom-one */
					/* BEGINNING OF ACTION: ast-make-atom */
					{
#line 696 "src/libre/parser.act"

		(ZInode) = ast_make_expr_with_count((ZIe), (ZIc));
		if ((ZInode) == NULL) {
			err->e = RE_EXEOF;
			goto ZL6;
		}
	
#line 2065 "src/libre/dialect/native/parser.c"
					}
					/* END OF ACTION: ast-make-atom */
				}
				break;
			}
			goto ZL5;
		ZL6:;
			{
				/* BEGINNING OF ACTION: err-expected-count */
				{
#line 476 "src/libre/parser.act"

		if (err->e == RE_ESUCCESS) {
			err->e = RE_EXCOUNT;
		}
		goto ZL1;
	
#line 2083 "src/libre/dialect/native/parser.c"
				}
				/* END OF ACTION: err-expected-count */
				ZInode = ZIe;
			}
		ZL5:;
		}
		/* END OF INLINE: 167 */
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_expr_C_Ccharacter_Hclass_C_Cclass_Hnamed(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
	{
		t_ast__class__id ZIid;
		t_pos ZI146;
		t_pos ZI147;

		switch (CURRENT_TERMINAL) {
		case (TOK_NAMED__CLASS):
			/* BEGINNING OF EXTRACT: NAMED_CLASS */
			{
#line 441 "src/libre/parser.act"

		ZIid = DIALECT_CLASS(lex_state->buf.a);
		if (ZIid == NULL) {
			/* syntax error -- unrecognized class */
			goto ZL1;
		}

		ZI146 = lex_state->lx.start;
		ZI147   = lex_state->lx.end;
	
#line 2128 "src/libre/dialect/native/parser.c"
			}
			/* END OF EXTRACT: NAMED_CLASS */
			break;
		default:
			goto ZL1;
		}
		ADVANCE_LEXER;
		/* BEGINNING OF ACTION: ast-make-named */
		{
#line 824 "src/libre/parser.act"

		(ZInode) = ast_make_expr_named((ZIid));
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 2145 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: ast-make-named */
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

static void
p_expr_C_Calt(flags flags, lex_state lex_state, act_state act_state, err err, t_ast__expr *ZOnode)
{
	t_ast__expr ZInode;

	if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
		return;
	}
	{
		/* BEGINNING OF ACTION: ast-make-concat */
		{
#line 664 "src/libre/parser.act"

		(ZInode) = ast_make_expr_concat();
		if ((ZInode) == NULL) {
			goto ZL1;
		}
	
#line 2175 "src/libre/dialect/native/parser.c"
		}
		/* END OF ACTION: ast-make-concat */
		p_expr_C_Clist_Hof_Hatoms (flags, lex_state, act_state, err, ZInode);
		if ((CURRENT_TERMINAL) == (ERROR_TERMINAL)) {
			RESTORE_LEXER;
			goto ZL1;
		}
	}
	goto ZL0;
ZL1:;
	SAVE_LEXER ((ERROR_TERMINAL));
	return;
ZL0:;
	*ZOnode = ZInode;
}

/* BEGINNING OF TRAILER */

#line 984 "src/libre/parser.act"


	static int
	lgetc(struct LX_STATE *lx)
	{
		struct lex_state *lex_state;

		assert(lx != NULL);
		assert(lx->getc_opaque != NULL);

		lex_state = lx->getc_opaque;

		assert(lex_state->f != NULL);

		return lex_state->f(lex_state->opaque);
	}

	struct ast *
	DIALECT_PARSE(re_getchar_fun *f, void *opaque,
		const struct fsm_options *opt,
		enum re_flags flags, int overlap,
		struct re_err *err)
	{
		struct ast *ast;
		struct flags top, *fl = &top;

		struct act_state  act_state_s;
		struct act_state *act_state;
		struct lex_state  lex_state_s;
		struct lex_state *lex_state;
		struct re_err dummy;

		struct LX_STATE *lx;

		top.flags = flags;

		assert(f != NULL);

		ast = ast_new();

		if (err == NULL) {
			err = &dummy;
		}

		lex_state    = &lex_state_s;
		lex_state->p = lex_state->a;

		lx = &lex_state->lx;

		LX_INIT(lx);

		lx->lgetc       = lgetc;
		lx->getc_opaque = lex_state;

		lex_state->f       = f;
		lex_state->opaque  = opaque;

		lex_state->buf.a   = NULL;
		lex_state->buf.len = 0;

		/* XXX: unneccessary since we're lexing from a string */
		/* (except for pushing "[" and "]" around ::group-$dialect) */
		lx->buf_opaque = &lex_state->buf;
		lx->push       = CAT(LX_PREFIX, _dynpush);
		lx->clear      = CAT(LX_PREFIX, _dynclear);
		lx->free       = CAT(LX_PREFIX, _dynfree);

		/* This is a workaround for ADVANCE_LEXER assuming a pointer */
		act_state = &act_state_s;

		act_state->overlap = overlap;

		err->e = RE_ESUCCESS;

		ADVANCE_LEXER;
		DIALECT_ENTRY(fl, lex_state, act_state, err, &ast->expr);

		lx->free(lx->buf_opaque);

		if (err->e != RE_ESUCCESS) {
			/* TODO: free internals allocated during parsing (are there any?) */
			goto error;
		}

		if (ast->expr == NULL) {
			/* We shouldn't get here, it means there's error
			 * checking missing elsewhere. */
			if (err->e == RE_ESUCCESS) { assert(!"unreached"); }
			goto error;
		}

		return ast;

	error:

		/*
		 * Some errors describe multiple tokens; for these, the start and end
		 * positions belong to potentially different tokens, and therefore need
		 * to be stored statefully (in act_state). These are all from
		 * non-recursive productions by design, and so a stack isn't needed.
		 *
		 * Lexical errors describe a problem with a single token; for these,
		 * the start and end positions belong to that token.
		 *
		 * Syntax errors occur at the first point the order of tokens is known
		 * to be incorrect, rather than describing a span of bytes. For these,
		 * the start of the next token is most relevant.
		 */

		switch (err->e) {
		case RE_ENEGRANGE: err->start = act_state->rangestart; err->end = act_state->rangeend; break;
		case RE_ENEGCOUNT: err->start = act_state->countstart; err->end = act_state->countend; break;

		case RE_EHEXRANGE:
		case RE_EOCTRANGE:
		case RE_ECOUNTRANGE:
			/*
			 * Lexical errors: These are always generated for the current token,
			 * so lx->start/end here is correct because ADVANCE_LEXER has
			 * not been called.
			 */
			mark(&err->start, &lx->start);
			mark(&err->end,   &lx->end);
			break;

		default:
			/*
			 * Due to LL(1) lookahead, lx->start/end is the next token.
			 * This is approximately correct as the position of an error,
			 * but to be exactly correct, we store the pos for the previous token.
			 * This is more visible when whitespace exists.
			 */
			err->start = act_state->synstart;
			err->end   = act_state->synstart; /* single point */
			break;
		}

		ast_free(ast);

		return NULL;
	}

#line 2337 "src/libre/dialect/native/parser.c"

/* END OF FILE */
