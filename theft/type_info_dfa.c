#include "type_info_dfa.h"

static enum theft_alloc_res
dfa_alloc(struct theft *t, void *unused_env, void **output)
{
	(void)unused_env;
	const struct dfa_spec_env *env = theft_hook_get_env(t);
	assert(env->tag == 'D');
	const uint8_t state_ceil_bits = (env->state_ceil_bits
	    ? env->state_ceil_bits : DEF_STATE_CEIL_BITS);
	const uint8_t symbol_ceil_bits = (env->symbol_ceil_bits
	    ? env->symbol_ceil_bits : DEF_SYMBOL_CEIL_BITS);
	const uint8_t end_bits = (env->end_bits
	    ? env->end_bits : DEF_END_BITS);

	if (end_bits >= 64) {
		return THEFT_ALLOC_ERROR;
	}

	const size_t state_count = theft_random_bits(t, state_ceil_bits);

	bool has_start = false;
	struct dfa_spec *res = NULL;
	res = calloc(1, sizeof(*res));
	if (res == NULL) { return THEFT_ALLOC_ERROR; }

	res->states = calloc(state_count, sizeof(res->states[0]));
	if (res->states == NULL) {
		free(res);
		return THEFT_ALLOC_ERROR;
	}

	const uint64_t end_value = (1ULL << end_bits) - 1;

	res->state_count = 0;
	for (size_t s_i = 0; s_i < state_count; s_i++) {
		struct dfa_spec_state *s = &res->states[res->state_count];
		memset(s, 0x00, sizeof(*s));

		for (size_t i = 0; i < MAX_EDGES; i++) {
			struct dfa_spec_edge *e = &s->edges[s->edge_count];
			e->symbol = theft_random_bits(t, symbol_ceil_bits);
			e->state = theft_random_bits(t, state_ceil_bits);

			if (theft_random_bits(t, 3) > 0) {
				s->edge_count++; /* keep it */
			}
		}

		/* If the max value is drawn, mark it as an end state. */
		if (theft_random_bits(t, end_bits) == end_value) {
			s->end = true;
		}

		/* As this shrinks towards 0, states get discarded,
		 * but their contents still generate -- this keeps
		 * the DFA from changing dramatically during shrinking
		 * due to sudden misalignment of the random bits. */
		if (theft_random_bits(t, 3) > 0) {
			s->used = true;
			if (!has_start) {
				res->start = res->state_count;
				has_start = true;
			}
			res->state_count++; /* keep it */
		}
	}

	/* second pass: remove any edges pointing to nonexistent states
	 * or using symbols that already appear on the same state */
	for (size_t s_i = 0; s_i < state_count; s_i++) {
		struct dfa_spec_state *s = &res->states[s_i];
		if (!s->used) { continue; }

		uint64_t symbol_seen[4] = { 0 };

		struct dfa_spec_edge {
			unsigned char symbol;
			fsm_state_t state;
		} dst[MAX_EDGES] = { 0 };

		/* filter, only keep valid edges */
		size_t e_dst = 0;
		for (size_t e_src = 0; e_src < s->edge_count; e_src++) {
			bool keep;

			const fsm_state_t to = s->edges[e_src].state;
			const unsigned char symbol = s->edges[e_src].symbol;

			if (to >= res->state_count || !res->states[to].used) {
				keep = false; /* unused state */
			} else if (BITSET_CHECK(symbol_seen, symbol)) {
				keep = false; /* symbol already in use */
			} else {
				keep = true;
				BITSET_SET(symbol_seen, symbol);
			}

			if (keep) {
				memcpy(&dst[e_dst],
				    &s->edges[e_src], sizeof(*dst));
			        e_dst++;
			}
		}
		memset(s->edges, 0x00, MAX_EDGES * sizeof(*s->edges));
		memcpy(&s->edges[0], &dst[0],
		    e_dst * sizeof(dst[0]));
		s->edge_count = e_dst;
	}

	*output = res;
	return THEFT_ALLOC_OK;
}

static void
dfa_free(void *instance, void *env)
{
	(void)env;
	struct dfa_spec *dfa = instance;
	free(dfa->states);
	free(dfa);
}

static void
dfa_print(FILE *f, const void *instance, void *env)
{
	(void)env;
	const struct dfa_spec *dfa = instance;
	fprintf(f, "=== DFA#%p:\n", (void *)dfa);
	for (size_t s_i = 0; s_i < dfa->state_count; s_i++) {
		const struct dfa_spec_state *s = &dfa->states[s_i];
		if (!s->used) { continue; }
		fprintf(f, " -- state[%zu], %u edge%s",
		    s_i, s->edge_count,
		    s->edge_count == 1 ? "" : "s");
		if (s_i == dfa->start) {
			fprintf(f, ", start");
		}
		if (s->end) {
			fprintf(f, ", end");
		}
		fprintf(f, ":\n");
		for (size_t e_i = 0; e_i < s->edge_count; e_i++) {
			fprintf(f, "    -- %zu: 0x%x -> %u\n",
			    e_i, s->edges[e_i].symbol, s->edges[e_i].state);
		}
	}
}

const struct theft_type_info type_info_dfa = {
	.alloc = dfa_alloc,
	.free  = dfa_free,
	.print = dfa_print,
	.autoshrink_config = {
		.enable = true,
	}
};

struct fsm *
dfa_spec_build_fsm(const struct dfa_spec *spec)
{
	struct fsm *fsm = fsm_new(NULL);
	if (fsm == NULL) {
		fprintf(stderr, "-- ERROR: fsm_new\n");
		goto cleanup;
	}

	if (!fsm_addstate_bulk(fsm, spec->state_count)) {
		fprintf(stderr, "-- ERROR: fsm_addstate_bulk\n");
		goto cleanup;
	}

	if (spec->start < spec->state_count) {
		fsm_setstart(fsm, spec->start);
	}

	for (size_t s_i = 0; s_i < spec->state_count; s_i++) {
		const fsm_state_t state_id = (fsm_state_t)s_i;
		const struct dfa_spec_state *s = &spec->states[s_i];
		if (!s->used) { continue; }

		if (s->end) {
			fsm_setend(fsm, state_id, 1);
		}

		for (size_t e_i = 0; e_i < s->edge_count; e_i++) {
			const fsm_state_t to = s->edges[e_i].state;
			const unsigned char symbol = s->edges[e_i].symbol;
			if (!fsm_addedge_literal(fsm,
				state_id, to, symbol)) {
				fprintf(stderr, "-- ERROR: fsm_addedge_literal\n");
				goto cleanup;
			}
		}
	}

	if (!fsm_all(fsm, fsm_isdfa)) {
		fprintf(stderr, "-- ERROR: all is_dfa\n");
		goto cleanup;
	}

	return fsm;

cleanup:
	if (fsm != NULL) { fsm_free(fsm); }
	return NULL;
}
