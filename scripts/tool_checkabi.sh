#!/usr/bin/env bash
###############################################################################
# Copyright (c) Intel Corporation - All rights reserved.                      #
# This file is part of the LIBXSMM library.                                   #
#                                                                             #
# For information on the license, see the LICENSE file.                       #
# Further information: https://github.com/libxsmm/libxsmm/                    #
# SPDX-License-Identifier: BSD-3-Clause                                       #
###############################################################################
# Hans Pabst (Intel Corp.)
###############################################################################

HERE=$(cd "$(dirname "$0")" && pwd -P)
LIBS=${HERE}/../lib

#EXCLUDE="libxsmmgen"
INCLUDE="libxsmmf"
#INCLUDE="*"
ABINEW=.abi.log
ABITMP=.abi.tmp
ABICUR=.abi.txt

if [ -e "${LIBS}"/${INCLUDE}.so ]; then
  LIBARGS="${LIBARGS} -D"
  LIBTYPE=so
elif [ -e "${LIBS}"/${INCLUDE}.a ]; then
  LIBTYPE=a
else
  LIBTYPE=lib
fi

BASENAME=$(command -v basename)
SORT=$(command -v sort)
DIFF=$(command -v diff)
SED=$(command -v gsed)
CUT=$(command -v cut)
LS=$(command -v ls)
WC=$(command -v wc)
CP=$(command -v cp)
MV=$(command -v mv)
NM=$(command -v nm)

# GNU sed is desired (macOS)
if [ ! "${SED}" ]; then
  SED=$(command -v sed)
fi

if [ "${NM}" ] && [ "${SED}"  ] && [ "${CUT}"  ] && \
   [ "${LS}" ] && [ "${CP}"   ] && [ "${MV}"   ] && \
   [ "${WC}" ] && [ "${SORT}" ] && [ "${DIFF}" ];
then
  # determine behavior of sort command
  export LC_ALL=C IFS=$'\n'
  if [ "0" != "$(${LS} -1 "${LIBS}"/${INCLUDE}.${LIBTYPE} 2>/dev/null | ${WC} -l)" ]; then
    if [ "${LIBARGS}" ]; then LIBARGS=${LIBARGS## }; fi
    ${CP} /dev/null ${ABINEW}
    for LIBFILE in "${LIBS}"/*."${LIBTYPE}"; do
      LIB=$(${BASENAME} "${LIBFILE}" .${LIBTYPE})
      if [ ! "${EXCLUDE}" ] || [ "$(${SED} "/\b${LIB}\b/d" <<<"${EXCLUDE}")" ]; then
        if [ ! "${CMD}" ]; then # try certain flags only once
          if ${NM} --defined-only -p "${LIBARGS}" "${LIBFILE}" >/dev/null 2>/dev/null; then
            CMD="${NM} --defined-only --no-sort ${LIBARGS}"
          else
            CMD="${NM} ${LIBARGS}"
          fi
        fi
        echo "Checking ${LIB}..."
        for LINE in $(eval "${CMD} ${LIBFILE} 2>/dev/null"); do
          SYMBOL=$(${SED} -n "/ T /p" <<<"${LINE}" | ${CUT} -d" " -f3)
          if [ "${SYMBOL}" ]; then
            # cleanup compiler-specific symbols (Intel Fortran, GNU Fortran)
            SYMBOL=$(${SED} <<<"${SYMBOL}" \
              -e "s/^libxsmm_mp_libxsmm_\(..*\)_/libxsmm_\1/" \
              -e "s/^__libxsmm_MOD_libxsmm_/libxsmm_/")
            if [ "$(${SED} -n "/^libxsmm[^.]/p" <<<"${SYMBOL}")" ]; then
              echo "${SYMBOL}" >>${ABINEW}
            elif [ "libxsmmnoblas" != "${LIB}" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^__libxsmm_MOD___/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^__wrap_..*/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^internal_/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^libxsmm._/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^.gem._/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^memalign/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^realloc/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^malloc/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^free/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^_init/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^_fini/p")" ] && \
                 [ ! "$(${SED} <<<"${SYMBOL}" -n "/^iJIT_/p")" ];
            then
              >&2 echo "ERROR: non-conforming function name"
              echo "${LIB} ->${SYMBOL}"
              exit 1
            fi
          fi
        done
      else
        echo "Excluded ${LIB}"
      fi
    done
    ${SORT} -u ${ABINEW} >${ABITMP}
    ${MV} ${ABITMP} ${ABINEW}
    if [ "so" != "${LIBTYPE}" ]; then
      echo "Note: LIBXSMM must be built with \"make STATIC=0 SYM|DBG=1\"!"
    fi
    REMOVED=$(${DIFF} --new-line-format="" --unchanged-line-format="" <(${SORT} ${ABICUR}) ${ABINEW})
    if [ ! "${REMOVED}" ]; then
      ${CP} ${ABINEW} ${ABICUR}
      echo "Successfully completed."
    else
      >&2 echo "ERROR: removed or renamed function(s)"
      echo "${REMOVED}"
      exit 1
    fi
  elif [ -e "${LIBS}"/${INCLUDE}.${LIBTYPE} ]; then
    >&2 echo "ERROR: ABI checker requires shared libraries (${LIBTYPE})."
    exit 1
  else
    >&2 echo "ERROR: ABI checker requires Fortran interface (${INCLUDE})."
    exit 1
  fi
else
  >&2 echo "ERROR: missing prerequisites!"
  exit 1
fi
