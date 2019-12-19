/*
 * Copyright 2019 Katherine Flavel
 *
 * See LICENCE for the full copyright terms.
 */

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <fsm/fsm.h>
#include <fsm/bool.h>

#include <re/re.h>

#include "ast.h"
#include "ast_analysis.h"
#include "ast_compile.h"

static struct fsm *
compile_subexpr(struct ast_expr *e, enum re_flags flags)
{
	struct fsm *fsm;
	struct ast ast;

	/*
	 * We're compiling these expressions in isolation just for sake of
	 * set subtraction. In general we wouldn't know if these nodes inherit
	 * any flags from the parent (since we haven't yet run analysis for
	 * the parent node and its ancestors).
	 *
	 * However we do need to run ast_analysis() for sake of possible anchors
	 * (and whatever other complexities) within general sub-expressions,
	 * and so we provide flags as if these are top-level nodes.
	 *
	 * We're compiling implicitly anchored, just because it makes for
	 * simpler FSM.
	 */

	/* we're just setting these temporarily for sake of ast_analysis() */
	e->flags = AST_FLAG_FIRST | AST_FLAG_LAST;

	ast.expr = e;

	if (ast_analysis(&ast) != AST_ANALYSIS_OK) {
		return 0;
	}

	fsm = ast_compile(&ast, flags | RE_ANCHORED, NULL, NULL);
	if (fsm == NULL) {
		return 0;
	}

	e->flags = 0x0;

	return fsm;
}

static int
rewrite(struct ast_expr *n, enum re_flags flags)
{
	if (n == NULL) {
		return 1;
	}

	/* flags haven't been set yet */
	assert(n->flags == 0x0);

	switch (n->type) {
	case AST_EXPR_CONCAT: {
		size_t i;

		for (i = 0; i < n->u.concat.count; i++) {
			if (!rewrite(n->u.concat.n[i], flags)) {
				return 0;
			}
		}

		/* remove empty children; these have no semantic effect */
		for (i = 0; i < n->u.concat.count; ) {
			if (n->u.concat.n[i]->type == AST_EXPR_EMPTY) {
				ast_expr_free(n->u.concat.n[i]);

				if (i + 1 < n->u.concat.count) {
					memmove(&n->u.concat.n[i], &n->u.concat.n[i + 1],
						(n->u.concat.count - i - 1) * sizeof *n->u.concat.n);
				}

				n->u.concat.count--;
				continue;
			}

			i++;
		}

		/* Fold in concat children; these have to be in-situ, because order matters */
		for (i = 0; i < n->u.concat.count; ) {
			if (n->u.concat.n[i]->type == AST_EXPR_CONCAT) {
				struct ast_expr *dead = n->u.concat.n[i];

				/* because of depth-first simplification */
				assert(dead->u.concat.count >= 1);

				if (n->u.concat.alloc < n->u.concat.count + dead->u.concat.count - 1) {
					void *tmp;

					tmp = realloc(n->u.concat.n,
						(n->u.concat.count + dead->u.concat.count - 1) * sizeof *n->u.concat.n);
					if (tmp == NULL) {
						return 0;
					}

					n->u.concat.n = tmp;

					n->u.concat.alloc = (n->u.concat.count + dead->u.concat.count - 1) * sizeof *n->u.concat.n;
				}

				/* move along our existing tail to make space */
				if (i + 1 < n->u.concat.count) {
					memmove(&n->u.concat.n[i + dead->u.concat.count], &n->u.concat.n[i + 1],
						(n->u.concat.count - i - 1) * sizeof *n->u.concat.n);
				}

				memcpy(&n->u.concat.n[i], dead->u.concat.n,
					dead->u.concat.count * sizeof *dead->u.concat.n);

				n->u.concat.count--;
				n->u.concat.count += dead->u.concat.count;

				dead->u.concat.count = 0;
				ast_expr_free(dead);

				continue;
			}

			i++;
		}

		if (n->u.concat.count == 0) {
			free(n->u.concat.n);

			goto empty;
		}

		if (n->u.concat.count == 1) {
			void *p = n->u.concat.n, *q = n->u.concat.n[0];
			*n = *n->u.concat.n[0];
			free(p);
			free(q);
			return 1;
		}

		return 1;
	}

	case AST_EXPR_ALT: {
		size_t i;

		for (i = 0; i < n->u.alt.count; i++) {
			if (!rewrite(n->u.alt.n[i], flags)) {
				return 0;
			}
		}

		/*
		 * Empty children do have a semantic effect; for dialects with anchors,
		 * they affect the anchoring. So we cannot remove those in general here.
		 */
		/* TODO: multiple redundant empty nodes can be removed though, leaving just one */

		/* Fold in alt children; these do not have to be in-situ, because
		 * order doesn't matter here. But we do it anyway to be tidy */
		for (i = 0; i < n->u.alt.count; ) {
			if (n->u.alt.n[i]->type == AST_EXPR_ALT) {
				struct ast_expr *dead = n->u.alt.n[i];

				/* because of depth-first simplification */
				assert(dead->u.alt.count >= 1);

				if (n->u.alt.alloc < n->u.alt.count + dead->u.alt.count - 1) {
					void *tmp;

					tmp = realloc(n->u.alt.n,
						(n->u.alt.count + dead->u.alt.count - 1) * sizeof *n->u.alt.n);
					if (tmp == NULL) {
						return 0;
					}

					n->u.alt.n = tmp;

					n->u.alt.alloc = (n->u.alt.count + dead->u.alt.count - 1) * sizeof *n->u.alt.n;
				}

				/* move along our existing tail to make space */
				if (i + 1 < n->u.alt.count) {
					memmove(&n->u.alt.n[i + dead->u.alt.count], &n->u.alt.n[i + 1],
						(n->u.alt.count - i - 1) * sizeof *n->u.alt.n);
				}

				memcpy(&n->u.alt.n[i], dead->u.alt.n,
					dead->u.alt.count * sizeof *dead->u.alt.n);

				n->u.alt.count--;
				n->u.alt.count += dead->u.alt.count;

				dead->u.alt.count = 0;
				ast_expr_free(dead);

				continue;
			}

			i++;
		}

		/* de-duplicate children */
		if (n->u.alt.count > 1) {
			for (i = 0; i < n->u.alt.count; ) {
				if (ast_contains_expr(n->u.alt.n[i], n->u.alt.n + i + 1, n->u.alt.count - i - 1)) {
					ast_expr_free(n->u.concat.n[i]);

					if (i + 1 < n->u.concat.count) {
						memmove(&n->u.concat.n[i], &n->u.concat.n[i + 1],
							(n->u.concat.count - i - 1) * sizeof *n->u.concat.n);
					}

					n->u.concat.count--;
					continue;
				}

				i++;
			}
		}

		if (n->u.alt.count == 0) {
			free(n->u.alt.n);

			goto empty;
		}

		if (n->u.alt.count == 1) {
			void *p = n->u.alt.n, *q = n->u.alt.n[0];
			*n = *n->u.alt.n[0];
			free(p);
			free(q);
			return 1;
		}

		return 1;
	}

	case AST_EXPR_GROUP:
		return rewrite(n->u.group.e, flags);

	case AST_EXPR_REPEATED:
		return rewrite(n->u.repeated.e, flags);

	case AST_EXPR_SUBTRACT: {
		int empty;

		if (!rewrite(n->u.subtract.a, flags)) {
			return 0;
		}

		/* If the lhs operand is empty, the result is always empty */
		if (n->u.subtract.a->type == AST_EXPR_EMPTY) {
			ast_expr_free(n->u.subtract.a);
			ast_expr_free(n->u.subtract.b);

			goto empty;
		}

		if (!rewrite(n->u.subtract.b, flags)) {
			return 0;
		}

		/* TODO: If the rhs operand is 00-ff, the result is always empty
		 * (unless RE_UNICODE is set) */

		/* TODO: optimisation for computing subtractions for simple cases here;
		 * this should be possible by walking AST nodes directly (and sorting
		 * alt lists where neccessary) */

		/*
		 * This implementation of subtraction in terms of FSM is fallback for
		 * the general case; we will always want to support subtraction for
		 * arbitrary regular languages in general (as opposed to just the
		 * subsets our dialects currently construct for character classes),
		 * because eventually some dialect may offer an operator for
		 * subtracting arbitrary sub-expressions.
		 *
		 * This is all quite expensive.
		 *
		 * We always need to pass in user-supplied flags, because these
		 * sub-expressions may be written to assume particular flags
		 * (for example subtraction of two case-insensitive sets may depend
		 * on case-insensitity to produce an empty result).
		 */
		{
			struct fsm *a, *b;
			struct fsm *q;

			a = compile_subexpr(n->u.subtract.a, flags);
			if (a == NULL) {
				return 0;
			}

			b = compile_subexpr(n->u.subtract.b, flags);
			if (b == NULL) {
				fsm_free(a);
				return 0;
			}

			q = fsm_subtract(a, b);
			if (q == NULL) {
				return 0;
			}

			empty = fsm_empty(q);

			fsm_free(q);
		}

		if (empty) {
			ast_expr_free(n->u.subtract.a);
			ast_expr_free(n->u.subtract.b);
			goto empty;
		}

		return 1;
	}

	default:
		return 1;
	}

empty:

	n->type = AST_EXPR_EMPTY;

	return 1;
}

int
ast_rewrite(struct ast *ast, enum re_flags flags)
{
	return rewrite(ast->expr, flags);
}

