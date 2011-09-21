/*
 * C++ implementation of timsort
 *
 * ported from http://cr.openjdk.java.net/~martin/webrevs/openjdk7/timsort/raw_files/new/src/share/classes/java/util/TimSort.java
 *
 * Copyright 2010 Fuji, Goro <gfuji@cpan.org>. All rights reserved.
 *
 * You can use and/or redistribute in the same term of the original license.
 */

#include <vector>
#include <cassert>
#include <iterator>
#include <algorithm>
#include <utility>

#ifndef NDEBUG
#include <iostream>
#define LOG(expr) (std::clog << __func__ << ": " << (expr) << std::endl)
#else
#define LOG(expr) ((void)0)
#endif

#if __cplusplus <= 199711L // not a C++11
namespace std {
    template <typename T>
    inline const T move(const T& rvalue) {
        return rvalue;
    }
}
#endif

template <typename RandomAccessIterator, typename Compare>
class TimSort {
    typedef RandomAccessIterator iter_t;
    typedef typename std::iterator_traits<iter_t>::value_type value_t;
    typedef typename std::iterator_traits<iter_t>::reference ref_t;
    typedef typename std::iterator_traits<iter_t>::difference_type diff_t;
    typedef Compare compare_t;

    static const int MIN_MERGE = 32;

    iter_t    first_;
    iter_t    last_;
    compare_t comp_;

    static const int MIN_GALLOP = 7;

    int minGallop_; // default to MIN_GALLOP

    static const int INITIAL_TMP_STORAGE_LENGTH = 256;

    std::vector<value_t> tmp_; // temp storage for merges

    std::vector<iter_t> runBase_;
    std::vector<diff_t> runLen_;

    TimSort(iter_t const lo, iter_t const hi, compare_t c)
            : first_(lo), last_(hi),
              comp_(c), minGallop_(MIN_GALLOP) {
        assert( lo <= hi );
        diff_t const len = (hi - lo);
        tmp_.reserve(
                len < 2 * INITIAL_TMP_STORAGE_LENGTH
                    ? len >> 1
                    : INITIAL_TMP_STORAGE_LENGTH );
        
        runBase_.reserve(40);
        runLen_.reserve(40);
    }

    public:
    static void sort(iter_t lo, iter_t hi, compare_t c) {
        assert( lo <= hi );

        diff_t nRemaining = (hi - lo);
        if(nRemaining < 2) {
            return; // nothing to do
        }

        if(nRemaining < MIN_MERGE) {
            const diff_t initRunLen = countRunAndMakeAscending(lo, hi, c);
            binarySort(lo, hi, lo + initRunLen, c);
        }

        TimSort ts(lo, hi, c);
        const diff_t minRun = minRunLength(nRemaining);
        do {
            diff_t runLen = countRunAndMakeAscending(lo, hi, c);

            if(runLen < minRun) {
                const diff_t force  = std::min(nRemaining, minRun);
                binarySort(lo, lo + force, lo + runLen, c);
                runLen = force;
            }

            ts.pushRun(lo, runLen);
            ts.mergeCollapse();

            lo         += runLen;
            nRemaining -= runLen;
        } while(nRemaining != 0);

        assert( lo == hi );
        ts.mergeForceCollapse();
        assert( ts.runBase_.size() == 1 );
        assert( ts.runLen_.size()  == 1 );
    }

    private:
    static void
    binarySort(iter_t const lo, iter_t const hi, iter_t start, compare_t const c) {
        assert( lo <= start && start <= hi );
        std::cout << "!!" << (hi - start) << std::endl;
        if(start == lo) {
            ++start;
        }
        for( ; start < hi; ++start ) {
            value_t pivot = std::move(*start);
 
            iter_t left  = lo;
            iter_t right = start;
            assert( left <= right );

            while( left < right ) {
                const iter_t mid = left + ( (right - left) >> 1);
                if(c(pivot, *mid) < 0) {
                    right = mid;
                }
                else {
                    left = mid + 1;
                }
            }
            assert( left == right );
            std::copy( left, start, left + 1 );
            *left = std::move(pivot);
        }
    }

    static diff_t
    countRunAndMakeAscending(iter_t lo, iter_t hi, compare_t c) {
        assert( lo < hi );

        iter_t runHi = lo + 1;
        if( runHi == hi ) {
            return 1;
        }
        
        if(c(*(runHi++), *lo) < 0) { // descending
            LOG("descending");
            while(runHi < hi && c(*runHi, *(runHi - 1)) < 0) {
                ++runHi;
            }
            std::reverse(lo, runHi);
        }
        else { // ascending
            LOG("ascending");
            while(runHi < hi && c(*runHi, *(runHi - 1)) >= 0) {
                ++runHi;
            }
        }

        return runHi - lo;
    }

    static diff_t
    minRunLength(diff_t n) {
        assert( n >= 0 );

        diff_t r = 0;
        while(n >= MIN_MERGE) {
            r |= (n & 1);
            n >>= 1;
        }
        return n + r;
    }

    void
    pushRun(iter_t const runBase, diff_t const runLen) {
        runBase_.push_back(runBase);
        runLen_.push_back(runLen);
    }

    void
    mergeCollapse() {
        while( runBase_.size() > 1 ) {
            diff_t n = runBase_.size() - 2;

            if(n > 0 && runLen_[n - 1] <= runLen_[n] + runLen_[n + 1]) {
                if(runLen_[n - 1] < runLen_[n + 1]) {
                    --n;
                }
                mergeAt(n);
            }
            else if(runLen_[n] <= runLen_[n + 1]) {
                mergeAt(n);
            }
            else {
                break;
            }
        }
    }

    void
    mergeForceCollapse() {
        while( runBase_.size() > 1 ) {
            diff_t n = runBase_.size() - 2;

            if(n > 0 && runLen_[n - 1] <= runLen_[n + 1]) {
                --n;
            }
            mergeAt(n);
        }

    }

    void
    mergeAt(const diff_t i) {
        const diff_t stackSize = runBase_.size();
        assert( stackSize >= 2 );
        assert( i >= 0 );
        assert( i == stackSize - 2 || i == stackSize - 3 );

        iter_t base1 = runBase_[i];
        diff_t len1  = runLen_ [i];
        iter_t base2 = runBase_[i + 1];
        diff_t len2  = runLen_ [i + 1];

        assert( len1 > 0 && len2 > 0 );
        assert( base1 + len1 == base2 );

        runLen_[i] = len1 + len2;

        if(i == stackSize - 3) {
            runBase_[i + 1] = runBase_[i + 2];
            runLen_ [i + 1] = runLen_ [i + 2];
        }

        runBase_.pop_back();
        runLen_.pop_back();

        diff_t k = gallopRight(*base2, base1, len1, 0, comp_);
        assert( k >= 0 );

        base1 += k;
        len1  -= k;

        if(len1 == 0) {
            return;
        }

        len2 = gallopLeft(*(base1 + (len1 - 1)), base2, len2, len2 - 1, comp_);
        assert( len2 >= 0 );
        if(len2 == 0) {
            return;
        }

        if(len1 <= len2) {
            mergeLo(base1, len1, base2, len2);
        }
        else {
            mergeHi(base1, len1, base2, len2);
        }
    }

    int
    gallopLeft(ref_t key, iter_t base, diff_t len, diff_t hint, compare_t c) {
        assert( len > 0 && hint >= 0 && hint < len );

        int lastOfs = 0;
        int ofs = 1;

        if(c(key, *(base + hint)) > 0) {
            const int maxOfs =  len - hint;
            while(ofs < maxOfs && c(key, *(base + (hint + ofs))) > 0) {
                lastOfs = ofs;
                ofs     = (ofs << 1) + 1;

                if(ofs <= 0) { // int overflow
                    ofs = maxOfs;
                }
            }
            if(ofs > maxOfs) {
                ofs = maxOfs;
            }

            lastOfs += hint;
            ofs     += hint;
        }
        else {
            const int maxOfs = hint + 1;
            while(ofs < maxOfs && c(key, *(base + (hint - ofs))) <= 0) {
                lastOfs = ofs;
                ofs     = (ofs << 1) + 1;
                
                if(ofs <= 0) {
                    ofs = maxOfs;
                }
            }
            if(ofs > maxOfs) {
                ofs = maxOfs;
            }

            const diff_t tmp = lastOfs;
            lastOfs = hint - ofs;
            ofs     = hint - tmp;
        }
        assert( -1 <= lastOfs && lastOfs < ofs && ofs <= len );

        ++lastOfs;
        while(lastOfs < ofs) {
            const diff_t m = lastOfs + ((ofs - lastOfs) >> 1);

            if(c(key, *(base + m)) > 0) {
                lastOfs = m + 1;
            }
            else {
                ofs = m;
            }
        }
        assert( lastOfs == ofs );
        return ofs;
    }
    
    int
    gallopRight(ref_t key, iter_t base, diff_t len, diff_t hint, compare_t c) {
        assert( len > 0 && hint >= 0 && hint < len );

        int lastOfs = 0;
        int ofs = 1;

        if(c(key, *(base + hint)) < 0) {
            const int maxOfs = hint + 1;
            while(ofs < maxOfs && c(key, *(base + (hint - ofs))) < 0) {
                lastOfs = ofs;
                ofs     = (ofs << 1) + 1;
                
                if(ofs <= 0) {
                    ofs = maxOfs;
                }
            }
            if(ofs > maxOfs) {
                ofs = maxOfs;
            }

            const diff_t tmp = lastOfs;
            lastOfs = hint - ofs;
            ofs     = hint - tmp;
        }
        else {
            const int maxOfs =  len - hint;
            while(ofs < maxOfs && c(key, *(base + (hint + ofs))) >= 0) {
                lastOfs = ofs;
                ofs     = (ofs << 1) + 1;

                if(ofs <= 0) { // int overflow
                    ofs = maxOfs;
                }
            }
            if(ofs > maxOfs) {
                ofs = maxOfs;
            }

            lastOfs += hint;
            ofs     += hint;
        }
        assert( -1 <= lastOfs && lastOfs < ofs && ofs <= len );

        ++lastOfs;
        while(lastOfs < ofs) {
            const diff_t m = lastOfs + ((ofs - lastOfs) >> 1);

            if(c(key, *(base + m)) > 0) {
                lastOfs = m + 1;
            }
            else {
                ofs = m;
            }
        }
        assert( lastOfs == ofs );
        return ofs;
    }

    void mergeLo(iter_t base1, diff_t len1, iter_t base2, diff_t len2) {
        assert( len1 > 0 && len2 > 0 && base1 + len1 == base2 );

        tmp_.reserve(len1);
        std::copy(base1, base1 + len1, tmp_.begin());

        iter_t cursor1 = tmp_.begin();
        iter_t cursor2 = base2;
        iter_t dest    = base1;

        *(dest++) = *(cursor2++);
        if(--len2 == 0) {
            std::copy(cursor1, cursor1 + len1, dest);
            return;
        }
        if(len1 == 1) {
            std::copy(cursor2, cursor2 + len2, dest);
            *(dest + len2) = *cursor1;
            return;
        }

        compare_t c   = comp_;
        int minGallop = minGallop_;

        // outer:
        while(true) {
            int count1 = 0;
            int count2 = 0;

            bool break_outer = false;
            do {
                assert( len1 > 1 && len2 > 0 );

                if(c(*cursor2, *cursor1) < 0) {
                    *(dest++) = *(cursor2++);
                    ++count2;
                    count1 = 0;
                    if(--len2 == 0) {
                        break_outer = true;
                        break;
                    }
                }
                else {
                    *(dest++) = *(cursor1++);
                    ++count1;
                    count2 = 0;
                    if(--len1 == 1) {
                        break_outer = true;
                        break;
                    }
                }
            } while( (count1 | count2) < minGallop );
            if(break_outer) {
                break;
            }

            do {
                assert( len1 > 1 && len2 > 0 );

                count1 = gallopRight(*cursor2, cursor1, len1, 0, c);
                if(count1 != 0) {
                    std::copy(cursor1, cursor1 + count1, dest);
                    dest    += count1;
                    cursor1 += count1;
                    len1    -= count1;

                    if(len1 <= 1) {
                        break_outer = true;
                        break;
                    }
                }
                *(dest++) = *(cursor2++);
                if(--len2 == 0) {
                    break_outer = true;
                    break;
                }

                count2 = gallopLeft(*cursor1, cursor2, len2, 0, c);
                if(count2 != 0) {
                    std::copy(cursor2, cursor2 + count2, dest);
                    dest    += count2;
                    cursor2 += count2;
                    len2    -= count2;
                    if(len2 == 0) {
                        break_outer = true;
                        break;
                    }
                }
                *(dest++) = *(cursor1++);
                if(--len1 == 1) {
                    break_outer = true;
                    break;
                }

                minGallop--;
            } while( (count1 >= MIN_GALLOP) | (count2 >= MIN_GALLOP) );
            if(break_outer) {
                break;
            }

            if(minGallop < 0) {
                minGallop = 0;
            }
            minGallop += 2;
        } // end of "outer" loop

        minGallop_ = std::max(minGallop, 1);

        if(len1 == 1) {
            assert( len2 > 0 );
            std::copy(cursor2, cursor2 + len2, dest);
            *(dest + len2) = *cursor1;
        }
        else if(len1 == 0) {
            // throw IllegalArgumentException(
            //     "Comparison method violates its general contract!"); 
            assert(0);
        }
        else {
            assert( len2 == 0 );
            assert( len1 > 1 );
            std::copy(cursor1, cursor1 + len1, dest);
        }
    }


    void mergeHi(iter_t base1, diff_t len1, iter_t base2, diff_t len2) {
        assert( len1 > 0 && len2 > 0 && base1 + len1 == base2 );

        tmp_.reserve(len2);
        std::copy(base2, base2 + len2, tmp_.begin());

        iter_t cursor1 = base1 + (len1 - 1);
        iter_t cursor2 = tmp_.begin() + (len2 = 1);
        iter_t dest    = base2 + len2 - 1;

        *(dest--) = *(cursor1--);
        if(--len1 == 0) {
            std::copy(tmp_.begin(), tmp_.begin() + len2, dest - (len2 - 1));
            return;
        }
        if(len2 == 1) {
            dest    -= len1;
            cursor1 -= len1;
            std::copy(cursor1 + 1, cursor1 + (1 + len1), dest + 1);
            *dest = *cursor2;
            return;
        }

        compare_t c   = comp_;
        int minGallop = minGallop_;

        // outer:
        while(true) {
            int count1 = 0;
            int count2 = 0;

            bool break_outer = false;
            do {
                assert( len1 > 0 && len2 > 1 );

                if(c(*cursor2, *cursor1) < 0) {
                    *(dest--) = *(cursor1--);
                    ++count1;
                    count2 = 0;
                    if(--len1 == 0) {
                        break_outer = true;
                        break;
                    }
                }
                else {
                    *(dest--) = *(cursor2--);
                    ++count2;
                    count1 = 0;
                    if(--len2 == 1) {
                        break_outer = true;
                        break;
                    }
                }
            } while( (count1 | count2) < minGallop );
            if(break_outer) {
                break;
            }

            do {
                assert( len1 > 1 && len2 > 0 );

                count1 = len1 - gallopRight(*cursor2, base1, len1, len1 - 1, c);
                if(count1 != 0) {
                    dest    -= count1;
                    cursor1 -= count1;
                    len1    -= count1;
                    std::copy(cursor1 + 1, cursor1 + (1 + count1), dest + 1);

                    if(len1 == 0) {
                        break_outer = true;
                        break;
                    }
                }
                *(dest--) = *(cursor2--);
                if(--len2 == 0) {
                    break_outer = true;
                    break;
                }

                count2 = len2 - gallopLeft(*cursor1, tmp_.begin(), len2, len2 - 1, c);
                if(count2 != 0) {
                    dest    -= count2;
                    cursor2 -= count2;
                    len2    -= count2;
                    std::copy(cursor2 + 1, cursor2 + (1 + count2), dest + 1);
                    if(len2 <= 1) {
                        break_outer = true;
                        break;
                    }
                }
                *(dest--) = *(cursor1--);
                if(--len1 == 1) {
                    break_outer = true;
                    break;
                }

                minGallop--;
            } while( (count1 >= MIN_GALLOP) | (count2 >= MIN_GALLOP) );
            if(break_outer) {
                break;
            }

            if(minGallop < 0) {
                minGallop = 0;
            }
            minGallop += 2;
        } // end of "outer" loop

        minGallop_ = std::max(minGallop, 1);

        if(len2 == 1) {
            assert( len1 > 0 );
            dest    -= len1;
            cursor1 -= len1;
            std::copy(cursor1 + 1, cursor1 + (1 + len1), dest + 1);
            *dest = *cursor2;
        }
        else if(len1 == 0) {
            // throw IllegalArgumentException(
            //     "Comparison method violates its general contract!"); 
            assert(0);
        }
        else {
            assert( len1 == 0 );
            assert( len2 > 1 );
            std::copy(tmp_.begin(), tmp_.begin() + len2, dest - (len2 - 1));
        }


    }
};

template<typename Iter, typename Compare>
static inline void timsort(Iter first, Iter last, Compare c) {
    TimSort<Iter, Compare>::sort(first, last, c);
}
