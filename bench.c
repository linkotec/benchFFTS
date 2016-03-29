#include "libbench2/bench-user.h"
#include "ffts/include/ffts.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

BEGIN_BENCH_DOC
BENCH_DOC("name", "ffts")
BENCH_DOC("version", "v0.9")
BENCH_DOC("year", "2016")
END_BENCH_DOC 

int
can_do(bench_problem *p)
{
    bench_tensor *sz = p->sz;
    int i;

    if (p->kind != PROBLEM_COMPLEX &&
        p->kind != PROBLEM_REAL) {
            return 0;
    }

    if (sz->rnk < 1) {
        return 0;
    }

    for (i = 0; i < sz->rnk; ++i) {
        if (!power_of_two(sz->dims[i].n)) {
            return 0;
        }
    }

    return 1;
}

void
cleanup(void)
{
    /* nothing to do */
}

void
doit(int iter, bench_problem *p)
{
    ffts_plan_t *q = p->userinfo;
    const void *in = p->in;
    void *out = p->out;
    int i;

    for (i = 0; i < iter; ++i) {
        ffts_execute(q, in, out);
    }
}

void
done(bench_problem *p)
{
    if (p->userinfo) {
        ffts_free(p->userinfo);
    }
}

static size_t*
extract_dims(bench_tensor *sz)
{
    size_t *dims;
    int i;

    dims = (size_t*) bench_malloc(sizeof(*dims) * sz->rnk);
    if (!dims) {
        return NULL;
    }

    for (i = 0; i < sz->rnk; ++i) {
        dims[i] = sz->dims[i].n;
    }

    return dims;
}

void
final_cleanup(void)
{
    /* nothing to do */
}

void
initial_cleanup(void)
{
    /* nothing to do */
}

void
main_init(int *argc, char ***argv)
{
    (void*) (argc);
    (void*) (argv);
}

void
setup(bench_problem *p)
{
    bench_tensor *sz = p->sz;
    ffts_plan_t *plan;
    size_t *dims;
    double tim;

    timer_start(USER_TIMER);

    switch (p->kind)
    {
    case PROBLEM_COMPLEX:
        if (sz->rnk == 1) {
            if (verbose > 2) {
                printf("using ffts_init_1d\n");
            }
            plan = ffts_init_1d(sz->dims[0].n, p->sign);
        } else if (sz->rnk == 2) {
            if (verbose > 2) {
                printf("using ffts_init_2d\n");
            }
            plan = ffts_init_2d(sz->dims[0].n, sz->dims[1].n, p->sign);
        } else {
            if (verbose > 2) {
                printf("using ffts_init_nd\n");
            }
            dims = extract_dims(sz);
            plan = ffts_init_nd(sz->rnk, dims, p->sign);
            bench_free(dims);
        }
        break;
    case PROBLEM_REAL:
        if (sz->rnk == 1) {
            if (verbose > 2) {
                printf("using ffts_init_1d_real\n");
            }
            plan = ffts_init_1d_real(sz->dims[0].n, p->sign);
        } else if (sz->rnk == 2) {
            if (verbose > 2) {
                printf("using ffts_init_2d_real\n");
            }
            plan = ffts_init_2d_real(sz->dims[0].n, sz->dims[1].n, p->sign);
        } else {
            if (verbose > 2) {
                printf("using ffts_init_nd_real\n");
            }
            dims = extract_dims(sz);
            plan = ffts_init_nd_real(sz->rnk, dims, p->sign);
            bench_free(dims);
        }
        break;
    default:
        BENCH_ASSERT(0);
    }

    tim = timer_stop(USER_TIMER);
    if (verbose > 1) {
        printf("planner time: %g s\n", tim);
    }

    p->userinfo = plan;
    BENCH_ASSERT(p->userinfo);
}