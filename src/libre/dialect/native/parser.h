/*
 * Automatically generated from the files:
 *	src/libre/dialect/native/parser.sid
 * and
 *	src/libre/parser.act
 * by:
 *	sid
 */

/* BEGINNING OF HEADER */

#line 304 "src/libre/parser.act"


	#include <re/re.h>

	typedef struct lex_state * lex_state;
	typedef struct act_state * act_state;

	typedef enum re_flags * flags;
	typedef struct re_err * err;

	typedef struct ast_expr * t_ast__expr;
#line 25 "src/libre/dialect/native/parser.h"

/* BEGINNING OF FUNCTION DECLARATIONS */

extern void p_re__native(flags, lex_state, act_state, err, t_ast__expr *);
/* BEGINNING OF TRAILER */

#line 1145 "src/libre/parser.act"


#line 35 "src/libre/dialect/native/parser.h"

/* END OF FILE */
