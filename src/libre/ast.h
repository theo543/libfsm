/*
 * Copyright 2018 Scott Vokes
 *
 * See LICENCE for the full copyright terms.
 */

#ifndef RE_AST_H
#define RE_AST_H

/*
 * This is a duplicate of struct lx_pos, but since we're linking to
 * code with several distinct lexers, there isn't a clear lexer.h
 * to include here. The parser sees both definitions, and will
 * build a `struct ast_pos` in the appropriate places.
 */
struct ast_pos {
	unsigned byte;
	unsigned line;
	unsigned col;
};

enum ast_expr_type {
	/* Reserve one value (0) indicating a freed expression. This value is
	 * intentionally unnamed: code that switches on n->type should be able
	 * to leave it out without triggering a compiler diagnostic. */
	AST_EXPR_EMPTY = 1,
	AST_EXPR_CONCAT,
	AST_EXPR_ALT,
	AST_EXPR_LITERAL,
	AST_EXPR_CODEPOINT,
	AST_EXPR_REPEAT,
	AST_EXPR_GROUP,
	AST_EXPR_ANCHOR,
	AST_EXPR_SUBTRACT,
	AST_EXPR_RANGE,
/*	AST_EXPR_TYPE, XXX: not implemented */
	AST_EXPR_TOMBSTONE
};

#define AST_COUNT_UNBOUNDED ((unsigned)-1)
struct ast_count {
	unsigned min;
	struct ast_pos start;
	unsigned max;
	struct ast_pos end;
};

enum ast_anchor_type {
	AST_ANCHOR_START,
	AST_ANCHOR_END
};

/*
 * Flags used during AST analysis for expression nodes:
 *
 * - AST_FLAG_FIRST
 *   The node can appear at the beginning of input,
 *   possibly preceded by other nullable nodes.
 *
 * - AST_FLAG_LAST
 *   This node can appear at the end of input, possibly
 *   followed by nullable nodes.
 *
 * - AST_FLAG_UNSATISFIABLE
 *   The node caused the regex to become unsatisfiable.
 *
 * - AST_FLAG_NULLABLE
 *   The node is not always evaluated, such as nodes that
 *   are repeated at least 0 times.
 *
 * - AST_FLAG_ALWAYS_CONSUMES
 *   The subtree always consumes input.
 *
 * - AST_FLAG_CAN_CONSUME
 *   The subtree is able to consume input, though it
 *   may not in all cases.
 *
 * - AST_FLAG_ANCHORED_START
 *   The subtree is anchored to the global start.
 *
 * - AST_FLAG_ANCHORED_END
 *   The subtree is anchored to the global end.
 *
 * - AST_FLAG_END_NL
 *   The subtree, which must also have AST_FLAG_ANCHORED_END set,
 *   ends with the PCRE end anchor that implicitly matches a single
 *   trailing newline.
 *
 * Not all are valid for all node types.
 */
enum ast_flags {
	AST_FLAG_FIRST           = 1 << 0,
	AST_FLAG_LAST            = 1 << 1,
	AST_FLAG_UNSATISFIABLE   = 1 << 2,
	AST_FLAG_NULLABLE        = 1 << 3,
	AST_FLAG_ALWAYS_CONSUMES = 1 << 4,
	AST_FLAG_CAN_CONSUME     = 1 << 5,
	AST_FLAG_ANCHORED_START  = 1 << 6,
	AST_FLAG_ANCHORED_END    = 1 << 7,
	AST_FLAG_END_NL          = 1 << 8,

	AST_FLAG_NONE = 0x00
};

enum ast_endpoint_type {
	AST_ENDPOINT_LITERAL,
	AST_ENDPOINT_CODEPOINT,
/*	AST_ENDPOINT_TYPE, XXX: not implemented */
	AST_ENDPOINT_NAMED
};

struct ast_endpoint {
	enum ast_endpoint_type type;
	union {
		struct {
			unsigned char c;
		} literal;

		struct {
			uint32_t u;
		} codepoint;

		struct {
			const struct class *class;
		} named;
	} u;
};

/*
 * The following regular expression fragments map to associated fsm states
 * as follows (transitions written in .fsm format):
 *
 *  ab    concat:      1 -> 3 "a"; 3 -> 2 "b";
 *  a|b   alt:         1 -> 2 "a"; 1 -> 2 "b";
 *  a     literal:     1 -> 1a; 2a -> 2;
 *  .     any:         1 -> 2 ?;
 *  (a)   group:       1 -> 1a; 2a -> 2;
 *  [abc] char-class:  1 -> 2 "a"; 1 -> 2 "b"; 1 -> 2 "c";
 */
struct ast_expr {
	enum ast_expr_type type;
	enum ast_flags flags;
	enum re_flags re_flags;

	union {
		struct {
			struct ast_expr *nextnode;
		} free;

		/* ordered sequence */
		struct {
			size_t count; /* used */
			size_t alloc; /* allocated */
			struct ast_expr **n;
		} concat;

		/* unordered set */
		struct {
			size_t count; /* used */
			size_t alloc; /* allocated */
			struct ast_expr **n;
		} alt;

		struct {
			char c;
		} literal;

		struct {
			uint32_t u;
		} codepoint;

		struct ast_expr_repeat {
			struct ast_expr *e;
			unsigned min;
			unsigned max;
		} repeat;

		struct {
			struct ast_expr *e;
			unsigned id;
		} group;

		struct {
			enum ast_anchor_type type;
			int is_end_nl;
		} anchor;

		struct {
			struct ast_expr *a;
			struct ast_expr *b;
		} subtract;

		struct {
			struct ast_endpoint from;
			struct ast_pos start;
			struct ast_endpoint to;
			struct ast_pos end;
		} range;

		struct {
			const struct class *class;
		} named;
	} u;
};

enum { AST_EXPR_POOL_SIZE = 64 };

struct ast_expr_pool {
	struct {
		struct ast_expr expr;

#if defined(ASAN)
		struct {
			void *ptr[8]; /* force 8-byte alignment on amd64 */
		} redzone;
#endif
	} pool[AST_EXPR_POOL_SIZE];

	struct ast_expr *nextnode;
	struct ast_expr_pool *nextpool;
	size_t count;
};

struct ast_expr *
ast_expr_pool_new(struct ast_expr_pool **poolp);

void
ast_pool_free(struct ast_expr_pool *pool);

/* Returns current global expression pool, setting
 * global to NULL.
 *
 * This is a hack.
 */
struct ast_expr_pool *
ast_expr_pool_save(void);

struct ast {
	struct ast_expr_pool *pool;
	struct ast_expr *expr;
	int has_unanchored_start;
	int has_unanchored_end;
};

extern struct ast_expr *ast_expr_tombstone;

struct ast *
ast_new(void);

void
ast_free(struct ast *ast);

struct ast_count
ast_make_count(unsigned min, const struct ast_pos *start,
	unsigned max, const struct ast_pos *end);

/*
 * Expressions
 */

void
ast_expr_free(struct ast_expr_pool *pool, struct ast_expr *n);

int
ast_expr_clone(struct ast_expr_pool **poolp, struct ast_expr **n);

int
ast_expr_cmp(const struct ast_expr *a, const struct ast_expr *b);

int
ast_expr_equal(const struct ast_expr *a, const struct ast_expr *b);

int
ast_contains_expr(const struct ast_expr *node, struct ast_expr * const *a, size_t n);

struct ast_expr *
ast_make_expr_empty(struct ast_expr_pool **poolp, enum re_flags re_flags);

struct ast_expr *
ast_make_expr_concat(struct ast_expr_pool **poolp, enum re_flags re_flags);

struct ast_expr *
ast_make_expr_alt(struct ast_expr_pool **poolp, enum re_flags re_flags);

int
ast_add_expr_alt(struct ast_expr *alt, struct ast_expr *node);

struct ast_expr *
ast_make_expr_literal(struct ast_expr_pool **poolp, enum re_flags re_flags, char c);

struct ast_expr *
ast_make_expr_codepoint(struct ast_expr_pool **poolp, enum re_flags re_flags, uint32_t u);

struct ast_expr *
ast_make_expr_repeat(struct ast_expr_pool **poolp, enum re_flags re_flags, struct ast_expr *e, struct ast_count count);

struct ast_expr *
ast_make_expr_group(struct ast_expr_pool **poolp, enum re_flags re_flags, struct ast_expr *e, unsigned id);

struct ast_expr *
ast_make_expr_anchor(struct ast_expr_pool **poolp, enum re_flags re_flags, enum ast_anchor_type type);

struct ast_expr *
ast_make_expr_subtract(struct ast_expr_pool **poolp, enum re_flags re_flags, struct ast_expr *a, struct ast_expr *b);

int
ast_add_expr_concat(struct ast_expr *cat, struct ast_expr *node);

struct ast_expr *
ast_make_expr_range(struct ast_expr_pool **poolp, enum re_flags re_flags,
	const struct ast_endpoint *from, struct ast_pos start,
	const struct ast_endpoint *to, struct ast_pos end);

struct ast_expr *
ast_make_expr_named(struct ast_expr_pool **poolp, enum re_flags re_flags, const struct class *class);

/* XXX: exposed for sake of re(1) printing an ast;
 * it's not part of the <re/re.h> API proper */
struct fsm_options;
struct ast *
re_parse(enum re_dialect dialect, int (*getc)(void *opaque), void *opaque,
	const struct fsm_options *opt,
	enum re_flags flags, struct re_err *err, int *unsatisfiable);

const char *
ast_node_type_name(enum ast_expr_type t);

int
ast_expr_is_literal(const struct ast_expr *e,
	int *anchor_start, int *anchor_end,
    char **s, size_t *n);

#endif
