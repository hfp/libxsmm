/******************************************************************************
* Copyright (c) Intel Corporation - All rights reserved.                      *
* This file is part of the LIBXSMM library.                                   *
*                                                                             *
* For information on the license, see the LICENSE file.                       *
* Further information: https://github.com/libxsmm/libxsmm/                    *
* SPDX-License-Identifier: BSD-3-Clause                                       *
******************************************************************************/
/* Hans Pabst (Intel Corp.)
******************************************************************************/
#include "libxsmm_gemm.h"
#include "generator_common.h"
#include "libxsmm_xcopy.h"
#include "libxsmm_hash.h"

#if !defined(LIBXSMM_GEMM_TASKGRAIN)
# define LIBXSMM_GEMM_TASKGRAIN 128
#endif
#if !defined(LIBXSMM_GEMM_FASTPATH) && defined(NDEBUG) && 1
# define LIBXSMM_GEMM_FASTPATH
#endif

#if (0 != LIBXSMM_SYNC) /** Locks for the batch interface (duplicated C indexes). */
# define LIBXSMM_GEMM_LOCKIDX(IDX, NPOT) LIBXSMM_MOD2(LIBXSMM_CRC32U(LIBXSMM_BLASINT_NBITS)(2507/*seed*/, &(IDX)), NPOT)
# define LIBXSMM_GEMM_LOCKPTR(PTR, NPOT) LIBXSMM_MOD2(LIBXSMM_CRCPTR(1975/*seed*/, PTR), NPOT)
# if !defined(LIBXSMM_GEMM_MAXNLOCKS)
#   define LIBXSMM_GEMM_MAXNLOCKS 1024
# endif
# if !defined(LIBXSMM_GEMM_LOCKFWD) && 1
#   define LIBXSMM_GEMM_LOCKFWD
# endif
# if LIBXSMM_LOCK_TYPE_ISPOD(LIBXSMM_GEMM_LOCK)
LIBXSMM_EXTERN_C typedef union internal_gemm_locktype {
  char pad[LIBXSMM_CACHELINE];
  LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK) state;
} internal_gemm_locktype;
# else
LIBXSMM_EXTERN_C typedef union internal_gemm_locktype {
  LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK) state;
} internal_gemm_locktype;
# endif
LIBXSMM_APIVAR_DEFINE(internal_gemm_locktype internal_gemm_lock[LIBXSMM_GEMM_MAXNLOCKS]);
LIBXSMM_APIVAR_DEFINE(unsigned int internal_gemm_nlocks); /* populated number of locks */
#endif

/* definition of corresponding variables */
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_dgemm_batch_strided_function libxsmm_original_dgemm_batch_strided_function);
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_sgemm_batch_strided_function libxsmm_original_sgemm_batch_strided_function);
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_dgemm_batch_function libxsmm_original_dgemm_batch_function);
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_sgemm_batch_function libxsmm_original_sgemm_batch_function);
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_dgemm_function libxsmm_original_dgemm_function);
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_sgemm_function libxsmm_original_sgemm_function);
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_dgemv_function libxsmm_original_dgemv_function);
LIBXSMM_APIVAR_PUBLIC_DEF(/*volatile*/libxsmm_sgemv_function libxsmm_original_sgemv_function);
/* definition of corresponding variables */
LIBXSMM_APIVAR_PUBLIC_DEF(unsigned int libxsmm_gemm_taskgrain);
LIBXSMM_APIVAR_PUBLIC_DEF(int libxsmm_gemm_tasks);
#if defined(LIBXSMM_WRAP)
LIBXSMM_APIVAR_PUBLIC_DEF(int libxsmm_gemm_wrap);
#endif


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(double, gemm_batch_strided)(
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda, const libxsmm_blasint* stride_a,
  const double* b, const libxsmm_blasint* ldb, const libxsmm_blasint* stride_b,
  const double* beta, double* c, const libxsmm_blasint* ldc, const libxsmm_blasint* stride_c,
  const libxsmm_blasint* batchsize)
{
#if (0 != LIBXSMM_BLAS)
  LIBXSMM_ASSERT(NULL != transa && NULL != transb && NULL != m && NULL != n && NULL != k);
  LIBXSMM_ASSERT(NULL != alpha && NULL != beta && NULL != batchsize);
  LIBXSMM_ASSERT(NULL != a && NULL != lda && NULL != stride_a);
  LIBXSMM_ASSERT(NULL != b && NULL != ldb && NULL != stride_b);
  LIBXSMM_ASSERT(NULL != c && NULL != ldc && NULL != stride_c);
  libxsmm_gemm_batch_blas(LIBXSMM_DATATYPE_F64, LIBXSMM_DATATYPE_F64, transa, transb,
    *m, *n, *k, alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    -1/*index_stride*/, 0/*index_base*/, *batchsize);
#else
  libxsmm_blas_error("dgemm_batch_strided")(transa, transb, m, n, k,
    alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    batchsize);
#endif
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(float, gemm_batch_strided)(
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda, const libxsmm_blasint* stride_a,
  const float* b, const libxsmm_blasint* ldb, const libxsmm_blasint* stride_b,
  const float* beta, float* c, const libxsmm_blasint* ldc, const libxsmm_blasint* stride_c,
  const libxsmm_blasint* batchsize)
{
#if (0 != LIBXSMM_BLAS)
  LIBXSMM_ASSERT(NULL != transa && NULL != transb && NULL != m && NULL != n && NULL != k);
  LIBXSMM_ASSERT(NULL != alpha && NULL != beta && NULL != batchsize);
  LIBXSMM_ASSERT(NULL != a && NULL != lda && NULL != stride_a);
  LIBXSMM_ASSERT(NULL != b && NULL != ldb && NULL != stride_b);
  LIBXSMM_ASSERT(NULL != c && NULL != ldc && NULL != stride_c);
  libxsmm_gemm_batch_blas(LIBXSMM_DATATYPE_F32, LIBXSMM_DATATYPE_F32, transa, transb,
    *m, *n, *k, alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    -1/*index_stride*/, 0/*index_base*/, *batchsize);
#else
  libxsmm_blas_error("sgemm_batch_strided")(transa, transb, m, n, k,
    alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    batchsize);
#endif
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(double, gemm_batch)(
  const char transa_array[], const char transb_array[], const libxsmm_blasint m_array[], const libxsmm_blasint n_array[], const libxsmm_blasint k_array[],
  const double alpha_array[], const double* a_array[], const libxsmm_blasint lda_array[], const double* b_array[], const libxsmm_blasint ldb_array[],
  const double beta_array[], double* c_array[], const libxsmm_blasint ldc_array[], const libxsmm_blasint* group_count, const libxsmm_blasint group_size[])
{
#if (0 != LIBXSMM_BLAS)
  libxsmm_blasint i, j = 0;
  LIBXSMM_ASSERT(NULL != transa_array && NULL != transb_array && NULL != group_count && NULL != group_size);
  LIBXSMM_ASSERT(NULL != m_array && NULL != n_array && NULL != k_array && NULL != lda_array && NULL != ldb_array && NULL != ldc_array);
  LIBXSMM_ASSERT(NULL != a_array && NULL != b_array && NULL != c_array && NULL != alpha_array && NULL != beta_array);
  for (i = 0; i < *group_count; ++i) {
    const libxsmm_blasint size = group_size[i];
    libxsmm_gemm_batch_blas(LIBXSMM_DATATYPE_F64, LIBXSMM_DATATYPE_F64,
      transa_array + i, transb_array + i, m_array[i], n_array[i], k_array[i], alpha_array + i,
      a_array + j, lda_array + i, NULL/*stride_a*/, b_array + j, ldb_array + i, NULL/*stride_b*/, beta_array + i,
      c_array + j, ldc_array + i, NULL/*stride_c*/, 0/*index_stride*/, 0/*index_base*/, size);
    j += size;
  }
#else
  libxsmm_blas_error("dgemm_batch")(transa_array, transb_array, m_array, n_array, k_array,
    alpha_array, a_array, lda_array, b_array, ldb_array, beta_array, c_array, ldc_array,
    group_count, group_size);
#endif
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(float, gemm_batch)(
  const char transa_array[], const char transb_array[], const libxsmm_blasint m_array[], const libxsmm_blasint n_array[], const libxsmm_blasint k_array[],
  const float alpha_array[], const float* a_array[], const libxsmm_blasint lda_array[], const float* b_array[], const libxsmm_blasint ldb_array[],
  const float beta_array[], float* c_array[], const libxsmm_blasint ldc_array[], const libxsmm_blasint* group_count, const libxsmm_blasint group_size[])
{
#if (0 != LIBXSMM_BLAS)
  libxsmm_blasint i;
  LIBXSMM_ASSERT(NULL != transa_array && NULL != transb_array && NULL != group_count && NULL != group_size);
  LIBXSMM_ASSERT(NULL != m_array && NULL != n_array && NULL != k_array && NULL != lda_array && NULL != ldb_array && NULL != ldc_array);
  LIBXSMM_ASSERT(NULL != a_array && NULL != b_array && NULL != c_array && NULL != alpha_array && NULL != beta_array);
  for (i = 0; i < *group_count; ++i) {
    const libxsmm_blasint size = group_size[i];
    libxsmm_gemm_batch_blas(LIBXSMM_DATATYPE_F32, LIBXSMM_DATATYPE_F32,
      transa_array + i, transb_array + i, m_array[i], n_array[i], k_array[i], alpha_array + i,
      a_array + i, lda_array + i, NULL/*stride_a*/, b_array + i, ldb_array + i, NULL/*stride_b*/, beta_array + i,
      c_array + i, ldc_array + i, NULL/*stride_c*/, 0/*index_stride*/, 0/*index_base*/, size);
  }
#else
  libxsmm_blas_error("sgemm_batch")(transa_array, transb_array, m_array, n_array, k_array,
    alpha_array, a_array, lda_array, b_array, ldb_array, beta_array, c_array, ldc_array,
    group_count, group_size);
#endif
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(double, gemm)(
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda, const double* b, const libxsmm_blasint* ldb,
  const double* beta, double* c, const libxsmm_blasint* ldc)
{
#if (0 != LIBXSMM_BLAS)
  if (libxsmm_original_dgemm_function != LIBXSMM_BLAS_FSYMBOL_REAL(double, gemm) &&
      libxsmm_original_dgemm_function != NULL)
  {
    libxsmm_original_dgemm_function(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
  }
  else
#endif
  {
    libxsmm_blas_error("dgemm")(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
    LIBXSMM_INLINE_XGEMM(double, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
  }
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(float, gemm)(
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda, const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
#if (0 != LIBXSMM_BLAS)
  if (libxsmm_original_sgemm_function != LIBXSMM_BLAS_FSYMBOL_REAL(float, gemm) &&
      libxsmm_original_sgemm_function != NULL)
  {
    libxsmm_original_sgemm_function(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
  }
  else
#endif
  {
    libxsmm_blas_error("sgemm")(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
    LIBXSMM_INLINE_XGEMM(float, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
  }
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(double, gemv)(
  const char* trans, const libxsmm_blasint* m, const libxsmm_blasint* n,
  const double* alpha, const double* a, const libxsmm_blasint* lda, const double* x, const libxsmm_blasint* incx,
  const double* beta, double* y, const libxsmm_blasint* incy)
{
#if (0 != LIBXSMM_BLAS)
  if (libxsmm_original_dgemv_function != LIBXSMM_BLAS_FSYMBOL_REAL(double, gemv) &&
      libxsmm_original_dgemv_function != NULL)
  {
    libxsmm_original_dgemv_function(trans, m, n, alpha, a, lda, x, incx, beta, y, incy);
  }
  else
#endif
  {
    libxsmm_blas_error("dgemv")(trans, m, n, alpha, a, lda, x, incx, beta, y, incy);
  }
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void LIBXSMM_BLAS_FSYMBOL_REAL(float, gemv)(
  const char* trans, const libxsmm_blasint* m, const libxsmm_blasint* n,
  const float* alpha, const float* a, const libxsmm_blasint* lda, const float* x, const libxsmm_blasint* incx,
  const float* beta, float* y, const libxsmm_blasint* incy)
{
#if (0 != LIBXSMM_BLAS)
  if (libxsmm_original_sgemv_function != LIBXSMM_BLAS_FSYMBOL_REAL(float, gemv) &&
      libxsmm_original_sgemv_function != NULL)
  {
    libxsmm_original_sgemv_function(trans, m, n, alpha, a, lda, x, incx, beta, y, incy);
  }
  else
#endif
  {
    libxsmm_blas_error("sgemv")(trans, m, n, alpha, a, lda, x, incx, beta, y, incy);
  }
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void __real_dgemm_batch_strided(
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda, const libxsmm_blasint* stride_a,
  const double* b, const libxsmm_blasint* ldb, const libxsmm_blasint* stride_b,
  const double* beta, double* c, const libxsmm_blasint* ldc, const libxsmm_blasint* stride_c,
  const libxsmm_blasint* batchsize)
{
  LIBXSMM_BLAS_FSYMBOL_REAL(double, gemm_batch_strided)(transa, transb, m, n, k,
    alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    batchsize);
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void __real_sgemm_batch_strided(
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda, const libxsmm_blasint* stride_a,
  const float* b, const libxsmm_blasint* ldb, const libxsmm_blasint* stride_b,
  const float* beta, float* c, const libxsmm_blasint* ldc, const libxsmm_blasint* stride_c,
  const libxsmm_blasint* batchsize)
{
  LIBXSMM_BLAS_FSYMBOL_REAL(float, gemm_batch_strided)(transa, transb, m, n, k,
    alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    batchsize);
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void __real_dgemm_batch(
  const char transa_array[], const char transb_array[], const libxsmm_blasint m_array[], const libxsmm_blasint n_array[], const libxsmm_blasint k_array[],
  const double alpha_array[], const double* a_array[], const libxsmm_blasint lda_array[], const double* b_array[], const libxsmm_blasint ldb_array[],
  const double beta_array[], double* c_array[], const libxsmm_blasint ldc_array[], const libxsmm_blasint* group_count, const libxsmm_blasint group_size[])
{
  LIBXSMM_BLAS_FSYMBOL_REAL(double, gemm_batch)(transa_array, transb_array, m_array, n_array, k_array,
    alpha_array, a_array, lda_array, b_array, ldb_array, beta_array, c_array, ldc_array,
    group_count, group_size);
}


LIBXSMM_API LIBXSMM_ATTRIBUTE_WEAK void __real_sgemm_batch(
  const char transa_array[], const char transb_array[], const libxsmm_blasint m_array[], const libxsmm_blasint n_array[], const libxsmm_blasint k_array[],
  const float alpha_array[], const float* a_array[], const libxsmm_blasint lda_array[], const float* b_array[], const libxsmm_blasint ldb_array[],
  const float beta_array[], float* c_array[], const libxsmm_blasint ldc_array[], const libxsmm_blasint* group_count, const libxsmm_blasint group_size[])
{
  LIBXSMM_BLAS_FSYMBOL_REAL(float, gemm_batch)(transa_array, transb_array, m_array, n_array, k_array,
    alpha_array, a_array, lda_array, b_array, ldb_array, beta_array, c_array, ldc_array,
    group_count, group_size);
}


LIBXSMM_API libxsmm_sink_function libxsmm_blas_error(const char* symbol)
{
  static int error_once = 0;
  LIBXSMM_BLAS_ERROR(symbol, &error_once);
  return libxsmm_sink;
}


LIBXSMM_API_INTERN void libxsmm_gemm_init(void)
{
#if defined(LIBXSMM_WRAP) /* determines if wrap is considered */
  { /* intercepted GEMMs (1: sequential and non-tiled, 2: parallelized and tiled) */
    const char *const env_wrap = getenv("LIBXSMM_GEMM_WRAP");
    libxsmm_gemm_wrap = ((NULL == env_wrap || 0 == *env_wrap) ? (LIBXSMM_WRAP) : atoi(env_wrap));
  }
#endif
#if (0 != LIBXSMM_SYNC)
  { /* initialize locks for the batch interface */
    const char *const env_nlocks = getenv("LIBXSMM_GEMM_NLOCKS");
    const int nlocks = ((NULL == env_nlocks || 0 == *env_nlocks) ? -1/*default*/ : atoi(env_nlocks));
    unsigned int i;
    LIBXSMM_LOCK_ATTR_TYPE(LIBXSMM_GEMM_LOCK) attr = { 0 };
    LIBXSMM_LOCK_ATTR_INIT(LIBXSMM_GEMM_LOCK, &attr);
    internal_gemm_nlocks = (unsigned int)LIBXSMM_UP2POT(0 > nlocks ? (LIBXSMM_GEMM_MAXNLOCKS) : LIBXSMM_MIN(nlocks, LIBXSMM_GEMM_MAXNLOCKS));
    for (i = 0; i < internal_gemm_nlocks; ++i) LIBXSMM_LOCK_INIT(LIBXSMM_GEMM_LOCK, &internal_gemm_lock[i].state, &attr);
    LIBXSMM_LOCK_ATTR_DESTROY(LIBXSMM_GEMM_LOCK, &attr);
  }
#endif
  { /* determines if OpenMP tasks are used (when available) */
    const char *const env_tasks = getenv("LIBXSMM_GEMM_TASKS");
    const int gemm_tasks = ((NULL == env_tasks || 0 == *env_tasks) ? 0/*disabled*/ : atoi(env_tasks));
    libxsmm_gemm_tasks = (0 <= gemm_tasks ? LIBXSMM_ABS(gemm_tasks) : 1/*enabled*/);
  }
  { /* determines grain-size of tasks (when available) */
    const char *const env_taskgrain = getenv("LIBXSMM_GEMM_TASKGRAIN");
    const int gemm_taskgrain = ((NULL == env_taskgrain || 0 == *env_taskgrain || 0 >= atoi(env_taskgrain))
      ? (LIBXSMM_GEMM_TASKGRAIN) : atoi(env_taskgrain));
    /* adjust grain-size or scale beyond the number of threads */
    libxsmm_gemm_taskgrain = LIBXSMM_MAX(0 < libxsmm_gemm_tasks
      ? (gemm_taskgrain / libxsmm_gemm_tasks) : gemm_taskgrain, 1);
  }
  { /* determine BLAS function-pointers */
    LIBXSMM_BLAS_WRAPPER(double, gemm_batch_strided, libxsmm_original_dgemm_batch_strided_function);
    LIBXSMM_BLAS_WRAPPER(float, gemm_batch_strided, libxsmm_original_sgemm_batch_strided_function);
    LIBXSMM_BLAS_WRAPPER(double, gemm_batch, libxsmm_original_dgemm_batch_function);
    LIBXSMM_BLAS_WRAPPER(float, gemm_batch, libxsmm_original_sgemm_batch_function);
    LIBXSMM_BLAS_WRAPPER(double, gemm, libxsmm_original_dgemm_function);
    LIBXSMM_BLAS_WRAPPER(float, gemm, libxsmm_original_sgemm_function);
    LIBXSMM_BLAS_WRAPPER(double, gemv, libxsmm_original_dgemv_function);
    LIBXSMM_BLAS_WRAPPER(float, gemv, libxsmm_original_sgemv_function);
  }
}


LIBXSMM_API_INTERN void libxsmm_gemm_finalize(void)
{
#if (0 != LIBXSMM_SYNC)
  unsigned int i; for (i = 0; i < internal_gemm_nlocks; ++i) LIBXSMM_LOCK_DESTROY(LIBXSMM_GEMM_LOCK, &internal_gemm_lock[i].state);
#endif
}


LIBXSMM_API libxsmm_gemm_prefetch_type libxsmm_get_gemm_prefetch(int prefetch)
{
  libxsmm_gemm_prefetch_type result;
  if (0 > prefetch) {
    LIBXSMM_INIT /* load configuration */
    result = LIBXSMM_GEMM_PREFETCH_NONE;
  }
  else {
    result = (libxsmm_gemm_prefetch_type)prefetch;
  }
  return result;
}


LIBXSMM_API void libxsmm_gemm_strided(libxsmm_datatype iprec, libxsmm_datatype oprec,
  const char* transa, const char* transb, libxsmm_blasint m, libxsmm_blasint n, libxsmm_blasint k,
  const void* alpha, const void* a, const libxsmm_blasint* lda, const libxsmm_blasint* stride_a,
                     const void* b, const libxsmm_blasint* ldb, const libxsmm_blasint* stride_b,
  const void* beta,        void* c, const libxsmm_blasint* ldc, const libxsmm_blasint* stride_c,
  libxsmm_blasint index_base, libxsmm_blasint batchsize)
{
  libxsmm_gemm_batch_task(iprec, oprec, transa, transb, m, n, k,
    alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    -1/*index_stride*/, index_base, batchsize, 0/*tid*/, 1/*ntasks*/);
}


LIBXSMM_API void libxsmm_gemm_groups(
  libxsmm_datatype iprec, libxsmm_datatype oprec, const char transa_array[], const char transb_array[],
  const libxsmm_blasint m_array[], const libxsmm_blasint n_array[], const libxsmm_blasint k_array[],
  const void* alpha_array, const void* a_array[], const libxsmm_blasint lda_array[],
                           const void* b_array[], const libxsmm_blasint ldb_array[],
  const void* beta_array,        void* c_array[], const libxsmm_blasint ldc_array[],
  const libxsmm_blasint ngroups, const libxsmm_blasint batchsize[])
{
  const unsigned char typesize = libxsmm_typesize(oprec);
  const char *const palpha = (const char*)alpha_array;
  const char *const pbeta = (const char*)beta_array;
  libxsmm_blasint i, j = 0, n = LIBXSMM_ABS(ngroups);
  for (i = 0; i < n; ++i) {
    const libxsmm_blasint size = batchsize[i], s = LIBXSMM_ABS(size);
    libxsmm_gemm_batch_task(iprec, oprec, transa_array + i, transb_array + i, m_array[i], n_array[i], k_array[i],
      palpha + i * typesize, a_array + j, lda_array + i, NULL/*stride_a*/, b_array + j, ldb_array + i, NULL/*stride_b*/,
      pbeta + i * typesize, c_array + j, ldc_array + i, NULL/*stride_c*/, 0/*index_stride*/, 0/*index_base*/,
      0 < ngroups ? size : -s, 0/*tid*/, 1/*ntasks*/);
    j += s;
  }
}


LIBXSMM_API void libxsmm_dgemm(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda,
  const double* b, const libxsmm_blasint* ldb,
  const double* beta, double* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_XGEMM(double, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


LIBXSMM_API void libxsmm_sgemm(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda,
  const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_XGEMM(float, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


LIBXSMM_API void libxsmm_blas_gemm(libxsmm_datatype iprec, libxsmm_datatype oprec,
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const void* alpha, const void* a, const libxsmm_blasint* lda, const void* b, const libxsmm_blasint* ldb,
  const void* beta, void* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_INIT
  switch ((int)iprec) {
    case LIBXSMM_DATATYPE_F64: {
      LIBXSMM_ASSERT(iprec == oprec);
      LIBXSMM_BLAS_XGEMM(double, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
    } break;
    case LIBXSMM_DATATYPE_F32: {
      LIBXSMM_ASSERT(iprec == oprec);
      LIBXSMM_BLAS_XGEMM(float, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
    } break;
    default: if (0 != libxsmm_verbosity) { /* library code is expected to be mute */
      static int error_once = 0;
      LIBXSMM_UNUSED(oprec);
      if (1 == LIBXSMM_ATOMIC_ADD_FETCH(&error_once, 1, LIBXSMM_ATOMIC_RELAXED)) { /* TODO: support I16, etc. */
        fprintf(stderr, "LIBXSMM ERROR: unsupported data-type requested!\n");
      }
    }
  }
}


LIBXSMM_API int libxsmm_gemm_batch_kernel(libxsmm_gemmfunction kernel, libxsmm_blasint index_base,
  libxsmm_blasint index_stride, const libxsmm_blasint stride_a[], const libxsmm_blasint stride_b[], const libxsmm_blasint stride_c[],
  const void* a, const void* b, void* c, libxsmm_blasint batchsize, /*unsigned*/int tid, /*unsigned*/int ntasks,
  unsigned char itypesize, unsigned char otypesize)
{
  int result = EXIT_SUCCESS;
  const libxsmm_blasint size = LIBXSMM_ABS(batchsize), nsplit = LIBXSMM_MIN(size, ntasks);
  const libxsmm_blasint tasksize = LIBXSMM_UPDIV(size, nsplit);
  const libxsmm_blasint begin = (tid < nsplit ? (tid * tasksize) : size), span = begin + tasksize;
  const libxsmm_blasint end = LIBXSMM_MIN(span, size);
  LIBXSMM_ASSERT(NULL != a && NULL != b && NULL != c && NULL != kernel);
  if (begin < end) {
    libxsmm_gemm_param gemm_param;
    char *a0, *b0, *const c0 = (char*)c;
    LIBXSMM_VALUE_ASSIGN(a0, a);
    LIBXSMM_VALUE_ASSIGN(b0, b);
#if defined(_DEBUG)
    memset(&gemm_param, 0, sizeof(libxsmm_gemm_param));
#endif
    LIBXSMM_ASSERT(0 < itypesize && 0 < otypesize);
    if ((libxsmm_blasint)sizeof(libxsmm_blasint) <= LIBXSMM_ABS(index_stride)) { /* stride arrays contain indexes */
      const libxsmm_blasint end1 = (end != size ? end : (end - 1)) * index_stride;
      libxsmm_blasint i = begin * index_stride;
#if (0 != LIBXSMM_SYNC)
      if (1 == nsplit || 0 == internal_gemm_nlocks || 0 > batchsize /*|| 0 != (LIBXSMM_GEMM_FLAG_BETA_0 & flags)*/)
#endif
      { /* no locking */
        libxsmm_blasint ai, bi, ci;
#if defined(LIBXSMM_GEMM_FASTPATH)
        const unsigned char ibits = (unsigned char)LIBXSMM_INTRINSICS_BITSCANBWD32(itypesize);
        const unsigned char obits = (unsigned char)LIBXSMM_INTRINSICS_BITSCANBWD32(otypesize);
        if (NULL != stride_a && NULL != stride_b && NULL != stride_c
          && itypesize == (1 << ibits) && otypesize == (1 << obits))
        {
          ai = LIBXSMM_VALUE1(const libxsmm_blasint, stride_a, i) - index_base;
          bi = LIBXSMM_VALUE1(const libxsmm_blasint, stride_b, i) - index_base;
          ci = LIBXSMM_VALUE1(const libxsmm_blasint, stride_c, i) - index_base;
# if defined(LIBXSMM_BATCH_CHECK)
          if (0 <= ai && 0 <= bi && 0 <= ci)
# endif
          {
            gemm_param.a.primary = &a0[ai << ibits];
            gemm_param.b.primary = &b0[bi << ibits];
            gemm_param.c.primary = &c0[ci << obits];
            for (i += index_stride; i <= end1; i += index_stride) {
              ai = LIBXSMM_VALUE1(const libxsmm_blasint, stride_a, i) - index_base;
              bi = LIBXSMM_VALUE1(const libxsmm_blasint, stride_b, i) - index_base;
              ci = LIBXSMM_VALUE1(const libxsmm_blasint, stride_c, i) - index_base;
# if defined(LIBXSMM_BATCH_CHECK)
              if (0 <= ai && 0 <= bi && 0 <= ci)
# endif
              {
                gemm_param.a.senary = &a0[ai << ibits];
                gemm_param.b.senary = &b0[bi << ibits];
                gemm_param.c.senary = &c0[ci << obits];
                kernel(&gemm_param);
                gemm_param.a.primary = gemm_param.a.senary; /* next */
                gemm_param.b.primary = gemm_param.b.senary; /* next */
                gemm_param.c.primary = gemm_param.c.senary; /* next */
              }
            }
          }
        }
        else
#endif
        { /* mixed specification of strides or general typesize */
          ai = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_a, i) - index_base;
          bi = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_b, i) - index_base;
          ci = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_c, i) - index_base;
#if defined(LIBXSMM_BATCH_CHECK)
          if (0 <= ai && 0 <= bi && 0 <= ci)
#endif
          {
            gemm_param.a.primary = &a0[ai * itypesize];
            gemm_param.b.primary = &b0[bi * itypesize];
            gemm_param.c.primary = &c0[ci * otypesize];
            for (i += index_stride; i <= end1; i += index_stride) {
              ai = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_a, i) - index_base;
              bi = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_b, i) - index_base;
              ci = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_c, i) - index_base;
#if defined(LIBXSMM_BATCH_CHECK)
              if (0 <= ai && 0 <= bi && 0 <= ci)
#endif
              {
                gemm_param.a.senary = &a0[ai * itypesize];
                gemm_param.b.senary = &b0[bi * itypesize];
                gemm_param.c.senary = &c0[ci * otypesize];
                kernel(&gemm_param);
                gemm_param.a.primary = gemm_param.a.senary; /* next */
                gemm_param.b.primary = gemm_param.b.senary; /* next */
                gemm_param.c.primary = gemm_param.c.senary; /* next */
              }
            }
          }
        }
        if ( /* remainder multiplication */
#if defined(LIBXSMM_BATCH_CHECK)
          0 <= ai && 0 <= bi && 0 <= ci &&
#endif
          end == size)
        {
          gemm_param.a.senary = gemm_param.a.primary;
          gemm_param.b.senary = gemm_param.b.primary;
          gemm_param.c.senary = gemm_param.c.primary;
          kernel(&gemm_param);
        }
      }
#if (0 != LIBXSMM_SYNC)
      else { /* synchronize among C-indexes */
        LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK)* lock = NULL;
# if defined(LIBXSMM_GEMM_LOCKFWD)
        LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK)* lock0 = NULL;
# endif
        libxsmm_blasint ai, bi, ci;
# if defined(LIBXSMM_GEMM_FASTPATH)
        const unsigned char ibits = (unsigned char)LIBXSMM_INTRINSICS_BITSCANBWD32(itypesize);
        const unsigned char obits = (unsigned char)LIBXSMM_INTRINSICS_BITSCANBWD32(otypesize);
        if (NULL != stride_a && NULL != stride_b && NULL != stride_c
          && itypesize == (1 << ibits) && otypesize == (1 << obits))
        {
          ai = LIBXSMM_VALUE1(const libxsmm_blasint, stride_a, i) - index_base;
          bi = LIBXSMM_VALUE1(const libxsmm_blasint, stride_b, i) - index_base;
          ci = LIBXSMM_VALUE1(const libxsmm_blasint, stride_c, i) - index_base;
#   if defined(LIBXSMM_BATCH_CHECK)
          if (0 <= ai && 0 <= bi && 0 <= ci)
#   endif
          {
            gemm_param.a.primary = &a0[ai << ibits];
            gemm_param.b.primary = &b0[bi << ibits];
            gemm_param.c.primary = &c0[ci << obits];
            lock = &internal_gemm_lock[LIBXSMM_GEMM_LOCKIDX(ci, internal_gemm_nlocks)].state;
            for (i += index_stride; i <= end1; i += index_stride) {
              ai = LIBXSMM_VALUE1(const libxsmm_blasint, stride_a, i) - index_base;
              bi = LIBXSMM_VALUE1(const libxsmm_blasint, stride_b, i) - index_base;
              ci = LIBXSMM_VALUE1(const libxsmm_blasint, stride_c, i) - index_base;
#   if defined(LIBXSMM_BATCH_CHECK)
              if (0 <= ai && 0 <= bi && 0 <= ci)
#   endif
              {
                LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK) *const lock1 = &internal_gemm_lock[LIBXSMM_GEMM_LOCKIDX(ci, internal_gemm_nlocks)].state;
                gemm_param.a.senary = &a0[ai << ibits];
                gemm_param.b.senary = &b0[bi << ibits];
                gemm_param.c.senary = &c0[ci << obits];
#   if defined(LIBXSMM_GEMM_LOCKFWD)
                if (lock != lock0) { lock0 = lock; LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock); }
#   else
                LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock);
#   endif
                kernel(&gemm_param);
#   if defined(LIBXSMM_GEMM_LOCKFWD)
                if (lock != lock1 || i == end1) { LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock); lock = lock1; }
#   else
                LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock); lock = lock1;
#   endif
                gemm_param.a.primary = gemm_param.a.senary; /* next */
                gemm_param.b.primary = gemm_param.b.senary; /* next */
                gemm_param.c.primary = gemm_param.c.senary; /* next */
              }
            }
          }
        }
        else
# endif
        { /* mixed specification of strides or general typesize */
          ai = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_a, i) - index_base;
          bi = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_b, i) - index_base;
          ci = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_c, i) - index_base;
# if defined(LIBXSMM_BATCH_CHECK)
          if (0 <= ai && 0 <= bi && 0 <= ci)
# endif
          {
            gemm_param.a.primary = &a0[ai * itypesize];
            gemm_param.b.primary = &b0[bi * itypesize];
            gemm_param.c.primary = &c0[ci * otypesize];
            lock = &internal_gemm_lock[LIBXSMM_GEMM_LOCKIDX(ci, internal_gemm_nlocks)].state;
            for (i += index_stride; i <= end1; i += index_stride) {
              ai = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_a, i) - index_base;
              bi = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_b, i) - index_base;
              ci = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_c, i) - index_base;
# if defined(LIBXSMM_BATCH_CHECK)
              if (0 <= ai && 0 <= bi && 0 <= ci)
# endif
              {
                LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK) *const lock1 = &internal_gemm_lock[LIBXSMM_GEMM_LOCKIDX(ci, internal_gemm_nlocks)].state;
                gemm_param.a.senary = &a0[ai * itypesize];
                gemm_param.b.senary = &b0[bi * itypesize];
                gemm_param.c.senary = &c0[ci * otypesize];
# if defined(LIBXSMM_GEMM_LOCKFWD)
                if (lock != lock0) { lock0 = lock; LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock); }
# else
                LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock);
# endif
                kernel(&gemm_param);
# if defined(LIBXSMM_GEMM_LOCKFWD)
                if (lock != lock1 || i == end1) { LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock); lock = lock1; }
# else
                LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock); lock = lock1;
# endif
                gemm_param.a.primary = gemm_param.a.senary; /* next */
                gemm_param.b.primary = gemm_param.b.senary; /* next */
                gemm_param.c.primary = gemm_param.c.senary; /* next */
              }
            }
          }
        }
        LIBXSMM_ASSERT(NULL != lock);
        if ( /* remainder multiplication */
# if defined(LIBXSMM_BATCH_CHECK)
          0 <= ai && 0 <= bi && 0 <= ci &&
# endif
          end == size)
        {
          gemm_param.a.senary = gemm_param.a.primary;
          gemm_param.b.senary = gemm_param.b.primary;
          gemm_param.c.senary = gemm_param.c.primary;
          LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock);
          kernel(&gemm_param);
          LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock);
        }
      }
#endif /*(0 != LIBXSMM_SYNC)*/
    }
    else if (0 == index_stride) { /* array of pointers to matrices (singular strides are measured in Bytes) */
      const libxsmm_blasint pointersize = (libxsmm_blasint)sizeof(void*); /* LIBXSMM_BITS/8 */
      const libxsmm_blasint da = (NULL != stride_a ? *stride_a : pointersize) - index_base * pointersize;
      const libxsmm_blasint db = (NULL != stride_b ? *stride_b : pointersize) - index_base * pointersize;
      const libxsmm_blasint dc = (NULL != stride_c ? *stride_c : pointersize) - index_base * pointersize;
      char *ai = &a0[begin * da], *bi = &b0[begin * db], *ci = &c0[begin * dc];
      const libxsmm_blasint end1 = (end != size ? end : (end - 1));
      libxsmm_blasint i;
      gemm_param.a.primary = *((void**)ai);
      gemm_param.b.primary = *((void**)bi);
      gemm_param.c.primary = *((void**)ci);
#if (0 != LIBXSMM_SYNC)
      if (1 == nsplit || 0 == internal_gemm_nlocks || 0 > batchsize /*|| 0 != (LIBXSMM_GEMM_FLAG_BETA_0 & flags)*/)
#endif
      { /* no locking */
        for (i = begin; i < end1; ++i) {
          char *const an = ai + da, *const bn = bi + db, *const cn = ci + dc;
          gemm_param.a.senary = *((void**)an);
          gemm_param.b.senary = *((void**)bn);
          gemm_param.c.senary = *((void**)cn);
#if defined(LIBXSMM_BATCH_CHECK)
          if (NULL != gemm_param.a.primary && NULL != gemm_param.b.primary && NULL != gemm_param.c.primary)
#endif
          {
            kernel(&gemm_param);
          }
          gemm_param.a.primary = gemm_param.a.senary; /* next */
          gemm_param.b.primary = gemm_param.b.senary; /* next */
          gemm_param.c.primary = gemm_param.c.senary; /* next */
          ai = an; bi = bn; ci = cn; /* next */
        }
        if ( /* remainder multiplication */
#if defined(LIBXSMM_BATCH_CHECK)
          NULL != gemm_param.a.primary && NULL != gemm_param.b.primary && NULL != gemm_param.c.primary &&
#endif
          end == size)
        {
          gemm_param.a.senary = gemm_param.a.primary;
          gemm_param.b.senary = gemm_param.b.primary;
          gemm_param.c.senary = gemm_param.c.primary;
          kernel(&gemm_param);
        }
      }
#if (0 != LIBXSMM_SYNC)
      else { /* synchronize among C-indexes */
        LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK)* lock = &internal_gemm_lock[LIBXSMM_GEMM_LOCKPTR(gemm_param.c.primary, internal_gemm_nlocks)].state;
# if defined(LIBXSMM_GEMM_LOCKFWD)
        LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK)* lock0 = NULL;
# endif
        LIBXSMM_ASSERT(NULL != lock);
        for (i = begin + 1; i <= end1; ++i) {
          char *const an = ai + da, *const bn = bi + db, *const cn = ci + dc;
          gemm_param.a.senary = *((void**)an);
          gemm_param.b.senary = *((void**)bn);
          gemm_param.c.senary = *((void**)cn);
# if defined(LIBXSMM_BATCH_CHECK)
          if (NULL != gemm_param.a.primary && NULL != gemm_param.b.primary && NULL != gemm_param.c.primary)
# endif
          {
            LIBXSMM_LOCK_TYPE(LIBXSMM_GEMM_LOCK) *const lock1 = &internal_gemm_lock[LIBXSMM_GEMM_LOCKPTR(gemm_param.c.senary, internal_gemm_nlocks)].state;
# if defined(LIBXSMM_GEMM_LOCKFWD)
            if (lock != lock0) { lock0 = lock; LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock); }
# else
            LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock);
# endif
            kernel(&gemm_param);
# if defined(LIBXSMM_GEMM_LOCKFWD)
            if (lock != lock1 || i == end1) { LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock); lock = lock1; }
# else
            LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock); lock = lock1;
# endif
          }
          gemm_param.a.primary = gemm_param.a.senary; /* next */
          gemm_param.b.primary = gemm_param.b.senary; /* next */
          gemm_param.c.primary = gemm_param.c.senary; /* next */
          ai = an; bi = bn; ci = cn; /* next */
        }
        if ( /* remainder multiplication */
# if defined(LIBXSMM_BATCH_CHECK)
          NULL != gemm_param.a.primary && NULL != gemm_param.b.primary && NULL != gemm_param.c.primary &&
# endif
          end == size)
        {
          gemm_param.a.senary = gemm_param.a.primary;
          gemm_param.b.senary = gemm_param.b.primary;
          gemm_param.c.senary = gemm_param.c.primary;
          LIBXSMM_LOCK_ACQUIRE(LIBXSMM_GEMM_LOCK, lock);
          kernel(&gemm_param);
          LIBXSMM_LOCK_RELEASE(LIBXSMM_GEMM_LOCK, lock);
        }
      }
#endif /*(0 != LIBXSMM_SYNC)*/
    }
    else { /* strided (never synchronized) */
      const libxsmm_blasint da = (NULL != stride_a ? ((*stride_a - index_base) * itypesize) : itypesize);
      const libxsmm_blasint db = (NULL != stride_b ? ((*stride_b - index_base) * itypesize) : itypesize);
      const libxsmm_blasint dc = (NULL != stride_c ? ((*stride_c - index_base) * otypesize) : otypesize);
      const libxsmm_blasint end1 = (end != size ? end : (end - 1));
      libxsmm_blasint i;
      gemm_param.a.primary = &a0[begin * da];
      gemm_param.b.primary = &b0[begin * db];
      gemm_param.c.primary = &c0[begin * dc];
      for (i = begin; i < end1; ++i) {
        gemm_param.a.senary = (char*)gemm_param.a.primary + da;
        gemm_param.b.senary = (char*)gemm_param.b.primary + db;
        gemm_param.c.senary = (char*)gemm_param.c.primary + dc;
        kernel(&gemm_param);
        gemm_param.a.primary = gemm_param.a.senary; /* next */
        gemm_param.b.primary = gemm_param.b.senary; /* next */
        gemm_param.c.primary = gemm_param.c.senary; /* next */
      }
      if (end == size) { /* remainder multiplication */
        gemm_param.a.senary = gemm_param.a.primary;
        gemm_param.b.senary = gemm_param.b.primary;
        gemm_param.c.senary = gemm_param.c.primary;
        kernel(&gemm_param);
      }
    }
  }
  /* coverity[missing_unlock] */
  return result;
}


LIBXSMM_API void libxsmm_gemm_batch_blas(libxsmm_datatype iprec, libxsmm_datatype oprec,
  const char* transa, const char* transb, libxsmm_blasint m, libxsmm_blasint n, libxsmm_blasint k,
  const void* alpha, const void* a, const libxsmm_blasint* lda, const libxsmm_blasint stride_a[],
  const void* b, const libxsmm_blasint* ldb, const libxsmm_blasint stride_b[],
  const void* beta, void* c, const libxsmm_blasint* ldc, const libxsmm_blasint stride_c[],
  libxsmm_blasint index_stride, libxsmm_blasint index_base, libxsmm_blasint batchsize)
{
#if defined(LIBXSMM_BATCH_CHECK)
  if (NULL != a && NULL != b && NULL != c && iprec == oprec)
#endif
  {
    const char *const a0 = (const char*)a, *const b0 = (const char*)b;
    char *const c0 = (char*)c;
    const libxsmm_blasint size = LIBXSMM_ABS(batchsize);
    libxsmm_blasint i = 0;
    if ((libxsmm_blasint)sizeof(libxsmm_blasint) <= LIBXSMM_ABS(index_stride)) { /* stride arrays contain indexes */
      const unsigned char itypesize = libxsmm_typesize(iprec), otypesize = libxsmm_typesize(oprec);
      const libxsmm_blasint end = size * index_stride;
      for (; i < end; i += index_stride) {
        const libxsmm_blasint ai = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_a, i) - index_base;
        const libxsmm_blasint bi = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_b, i) - index_base;
        const libxsmm_blasint ci = LIBXSMM_VALUE1_CHECKED(const libxsmm_blasint, index_base, stride_c, i) - index_base;
#if defined(LIBXSMM_BATCH_CHECK)
        if (0 <= ai && 0 <= bi && 0 <= ci)
#endif
        libxsmm_blas_gemm(iprec, oprec, transa, transb, &m, &n, &k,
          alpha, &a0[ai * itypesize], lda, &b0[bi * itypesize], ldb,
          beta, &c0[ci * otypesize], ldc);
      }
    }
    else if (0 == index_stride) { /* array of pointers to matrices (singular strides are measured in Bytes) */
      const libxsmm_blasint pointersize = (libxsmm_blasint)sizeof(void*); /* LIBXSMM_BITS/8 */
      const libxsmm_blasint da = (NULL != stride_a ? *stride_a : pointersize) - index_base * pointersize;
      const libxsmm_blasint db = (NULL != stride_b ? *stride_b : pointersize) - index_base * pointersize;
      const libxsmm_blasint dc = (NULL != stride_c ? *stride_c : pointersize) - index_base * pointersize;
      union { const void* pc; const void** ppc; } ai, bi;
      for (i = 0; i < size; ++i) {
        void *const ci = *(void**)&c0[i * dc];
        ai.pc = &a0[i * da]; bi.pc = &b0[i * db];
#if defined(LIBXSMM_BATCH_CHECK)
        if (NULL != *ai.ppc && NULL != *bi.ppc && NULL != ci)
#endif
        libxsmm_blas_gemm(iprec, oprec, transa, transb, &m, &n, &k, alpha, *ai.ppc, lda, *bi.ppc, ldb, beta, ci, ldc);
      }
    }
    else { /* strided */
      const unsigned char itypesize = libxsmm_typesize(iprec), otypesize = libxsmm_typesize(oprec);
      const libxsmm_blasint da = (NULL != stride_a ? ((*stride_a - index_base) * itypesize) : itypesize);
      const libxsmm_blasint db = (NULL != stride_b ? ((*stride_b - index_base) * itypesize) : itypesize);
      const libxsmm_blasint dc = (NULL != stride_c ? ((*stride_c - index_base) * otypesize) : otypesize);
      for (i = 0; i < size; ++i) {
        const void *const ai = &a0[i * da], *const bi = &b0[i * db];
        void *const ci = &c0[i * dc];
        libxsmm_blas_gemm(iprec, oprec, transa, transb, &m, &n, &k, alpha, ai, lda, bi, ldb, beta, ci, ldc);
      }
    }
  }
}


LIBXSMM_API void libxsmm_gemm_batch_task(libxsmm_datatype iprec, libxsmm_datatype oprec,
  const char* transa, const char* transb, libxsmm_blasint m, libxsmm_blasint n, libxsmm_blasint k,
  const void* alpha, const void* a, const libxsmm_blasint* lda, const libxsmm_blasint stride_a[],
  const void* b, const libxsmm_blasint* ldb, const libxsmm_blasint stride_b[],
  const void* beta, void* c, const libxsmm_blasint* ldc, const libxsmm_blasint stride_c[],
  libxsmm_blasint index_stride, libxsmm_blasint index_base, libxsmm_blasint batchsize,
  /*unsigned*/int tid, /*unsigned*/int ntasks)
{
#if defined(LIBXSMM_BATCH_CHECK)
  static int error_once = 0;
  if (NULL != a && NULL != b && NULL != c && 0 <= tid && tid < ntasks)
#endif
  {
    const unsigned char otypesize = libxsmm_typesize(oprec);
    int result = EXIT_SUCCESS;
    LIBXSMM_INIT
    if (LIBXSMM_SMM_AI(m, n, k, 2/*RFO*/, otypesize)) { /* check if an SMM is suitable */
      double dalpha = LIBXSMM_ALPHA, dbeta = LIBXSMM_BETA;
      result = libxsmm_dvalue(oprec, alpha, &dalpha);
      if (EXIT_SUCCESS == result) result = libxsmm_dvalue(oprec, beta, &dbeta);
      if (EXIT_SUCCESS == result) {
        const libxsmm_bitfield gemm_flags = LIBXSMM_GEMM_PFLAGS(transa, transb, LIBXSMM_FLAGS) |
          (LIBXSMM_NEQ(0, dbeta) ? 0 : LIBXSMM_GEMM_FLAG_BETA_0);
        const libxsmm_gemm_shape shape = libxsmm_create_gemm_shape(m, n, k,
          NULL != lda ? *lda : (0 == (LIBXSMM_GEMM_FLAG_TRANS_A & gemm_flags) ? m : k),
          NULL != ldb ? *ldb : (0 == (LIBXSMM_GEMM_FLAG_TRANS_B & gemm_flags) ? k : n),
          NULL != ldc ? *ldc : m, iprec, iprec, oprec, oprec);
        if (LIBXSMM_GEMM_NO_BYPASS(shape, dalpha, dbeta, gemm_flags)) {
          const libxsmm_bitfield prefetch = libxsmm_get_gemm_prefetch(LIBXSMM_PREFETCH_AUTO);
          libxsmm_xmmfunction kernel /*= { NULL }*/;
          kernel.gemm = libxsmm_dispatch_gemm(shape, gemm_flags, prefetch);
          if (NULL != kernel.ptr_const) {
            result = libxsmm_gemm_batch_kernel(kernel.gemm, index_base, index_stride,
              stride_a, stride_b, stride_c, a, b, c, batchsize, tid, ntasks,
              libxsmm_typesize(iprec), otypesize);
          }
          else result = EXIT_FAILURE;
        }
        else result = EXIT_FAILURE;
      }
    }
    else result = EXIT_FAILURE;
    if (EXIT_SUCCESS != result) { /* quiet fallback */
      libxsmm_gemm_batch_blas(iprec, oprec, transa, transb, m, n, k,
        alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
        index_stride, index_base, batchsize);
    }
  }
#if defined(LIBXSMM_BATCH_CHECK)
  else if (0 != libxsmm_verbosity /* library code is expected to be mute */
    && 1 == LIBXSMM_ATOMIC_ADD_FETCH(&error_once, 1, LIBXSMM_ATOMIC_RELAXED))
  {
    fprintf(stderr, "LIBXSMM ERROR: incorrect arguments (libxsmm_gemm_batch_task)!\n");
  }
#endif
}


LIBXSMM_API void libxsmm_gemm_batch(libxsmm_datatype iprec, libxsmm_datatype oprec,
  const char* transa, const char* transb, libxsmm_blasint m, libxsmm_blasint n, libxsmm_blasint k,
  const void* alpha, const void* a, const libxsmm_blasint* lda, const libxsmm_blasint stride_a[],
  const void* b, const libxsmm_blasint* ldb, const libxsmm_blasint stride_b[],
  const void* beta, void* c, const libxsmm_blasint* ldc, const libxsmm_blasint stride_c[],
  libxsmm_blasint index_stride, libxsmm_blasint index_base, libxsmm_blasint batchsize)
{
  libxsmm_gemm_batch_task(iprec, oprec, transa, transb, m, n, k,
    alpha,a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    index_stride, index_base, batchsize, 0/*tid*/, 1/*ntasks*/);
}


LIBXSMM_API_INTERN void libxsmm_sink(const void* arg, ...)
{ /* does nothing else but sinking given arguments */
  LIBXSMM_UNUSED(arg);
}


#if defined(LIBXSMM_BUILD) && (!defined(LIBXSMM_NOFORTRAN) || defined(__clang_analyzer__))

/* implementation provided for Fortran 77 compatibility */
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_dgemm)(const char*, const char*,
  const libxsmm_blasint*, const libxsmm_blasint*, const libxsmm_blasint*,
  const double*, const double*, const libxsmm_blasint*,
  const double*, const libxsmm_blasint*,
  const double*, double*, const libxsmm_blasint*);
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_dgemm)(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda,
  const double* b, const libxsmm_blasint* ldb,
  const double* beta, double* c, const libxsmm_blasint* ldc)
{
  libxsmm_dgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}

/* implementation provided for Fortran 77 compatibility */
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_sgemm)(const char*, const char*,
  const libxsmm_blasint*, const libxsmm_blasint*, const libxsmm_blasint*,
  const float*, const float*, const libxsmm_blasint*,
  const float*, const libxsmm_blasint*,
  const float*, float*, const libxsmm_blasint*);
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_sgemm)(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda,
  const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
  libxsmm_sgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


/* implementation provided for Fortran 77 compatibility */
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_blas_gemm)(const libxsmm_datatype*, const libxsmm_datatype*,
  const char*, const char*, const libxsmm_blasint*, const libxsmm_blasint*, const libxsmm_blasint*,
  const float*, const float*, const libxsmm_blasint*,
  const float*, const libxsmm_blasint*,
  const float*, float*, const libxsmm_blasint*);
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_blas_gemm)(const libxsmm_datatype* iprec, const libxsmm_datatype* oprec,
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda,
  const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_ASSERT(NULL != iprec && NULL != oprec);
  libxsmm_blas_gemm(*iprec, *oprec, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


/* implementation provided for Fortran 77 compatibility */
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_blas_dgemm)(const char*, const char*,
  const libxsmm_blasint*, const libxsmm_blasint*, const libxsmm_blasint*,
  const double*, const double*, const libxsmm_blasint*,
  const double*, const libxsmm_blasint*,
  const double*, double*, const libxsmm_blasint*);
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_blas_dgemm)(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda,
  const double* b, const libxsmm_blasint* ldb,
  const double* beta, double* c, const libxsmm_blasint* ldc)
{
  libxsmm_blas_dgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


/* implementation provided for Fortran 77 compatibility */
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_blas_sgemm)(const char*, const char*,
  const libxsmm_blasint*, const libxsmm_blasint*, const libxsmm_blasint*,
  const float*, const float*, const libxsmm_blasint*,
  const float*, const libxsmm_blasint*,
  const float*, float*, const libxsmm_blasint*);
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_blas_sgemm)(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda,
  const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
  libxsmm_blas_sgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


/* implementation provided for Fortran 77 compatibility */
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_gemm_batch_task)(const libxsmm_datatype* /*iprec*/, const libxsmm_datatype* /*oprec*/,
  const char* /*transa*/, const char* /*transb*/, const libxsmm_blasint* /*m*/, const libxsmm_blasint* /*n*/, const libxsmm_blasint* /*k*/,
  const void* /*alpha*/, const void* /*a*/, const libxsmm_blasint* /*lda*/, const libxsmm_blasint /*stride_a*/[],
  const void* /*b*/, const libxsmm_blasint* /*ldb*/, const libxsmm_blasint /*stride_b*/[],
  const void* /*beta*/, void* /*c*/, const libxsmm_blasint* /*ldc*/, const libxsmm_blasint /*stride_c*/[],
  const libxsmm_blasint* /*index_stride*/, const libxsmm_blasint* /*index_base*/,
  const libxsmm_blasint* /*batchsize*/, const /*unsigned*/int* /*tid*/, const /*unsigned*/int* /*ntasks*/);
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_gemm_batch_task)(const libxsmm_datatype* iprec, const libxsmm_datatype* oprec,
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const void* alpha, const void* a, const libxsmm_blasint* lda, const libxsmm_blasint stride_a[],
  const void* b, const libxsmm_blasint* ldb, const libxsmm_blasint stride_b[],
  const void* beta, void* c, const libxsmm_blasint* ldc, const libxsmm_blasint stride_c[],
  const libxsmm_blasint* index_stride, const libxsmm_blasint* index_base,
  const libxsmm_blasint* batchsize, const /*unsigned*/int* tid, const /*unsigned*/int* ntasks)
{
  LIBXSMM_ASSERT(NULL != iprec && NULL != oprec && NULL != m && NULL != n && NULL != k);
  LIBXSMM_ASSERT(NULL != index_base && NULL != index_stride && NULL != batchsize);
  LIBXSMM_ASSERT(NULL != tid && NULL != ntasks);
  libxsmm_gemm_batch_task(*iprec, *oprec, transa, transb, *m, *n, *k,
    alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    *index_stride, *index_base, *batchsize, *tid, *ntasks);
}


/* implementation provided for Fortran 77 compatibility */
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_gemm_batch)(const libxsmm_datatype* /*iprec*/, const libxsmm_datatype* /*oprec*/,
  const char* /*transa*/, const char* /*transb*/, const libxsmm_blasint* /*m*/, const libxsmm_blasint* /*n*/, const libxsmm_blasint* /*k*/,
  const void* /*alpha*/, const void* /*a*/, const libxsmm_blasint* /*lda*/, const libxsmm_blasint /*stride_a*/[],
  const void* /*b*/, const libxsmm_blasint* /*ldb*/, const libxsmm_blasint /*stride_b*/[],
  const void* /*beta*/, void* /*c*/, const libxsmm_blasint* /*ldc*/, const libxsmm_blasint /*stride_c*/[],
  const libxsmm_blasint* /*index_stride*/, const libxsmm_blasint* /*index_base*/, const libxsmm_blasint* /*batchsize*/);
LIBXSMM_API void LIBXSMM_FSYMBOL(libxsmm_gemm_batch)(const libxsmm_datatype* iprec, const libxsmm_datatype* oprec,
  const char* transa, const char* transb, const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const void* alpha, const void* a, const libxsmm_blasint* lda, const libxsmm_blasint stride_a[],
  const void* b, const libxsmm_blasint* ldb, const libxsmm_blasint stride_b[],
  const void* beta, void* c, const libxsmm_blasint* ldc, const libxsmm_blasint stride_c[],
  const libxsmm_blasint* index_stride, const libxsmm_blasint* index_base, const libxsmm_blasint* batchsize)
{
  LIBXSMM_ASSERT(NULL != iprec && NULL != oprec && NULL != m && NULL != n && NULL != k);
  LIBXSMM_ASSERT(NULL != index_base && NULL != index_stride && NULL != batchsize);
  libxsmm_gemm_batch(*iprec, *oprec, transa, transb, *m, *n, *k,
    alpha, a, lda, stride_a, b, ldb, stride_b, beta, c, ldc, stride_c,
    *index_stride, *index_base, *batchsize);
}

#endif /*defined(LIBXSMM_BUILD) && (!defined(LIBXSMM_NOFORTRAN) || defined(__clang_analyzer__))*/
