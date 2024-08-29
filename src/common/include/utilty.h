#ifndef SQK_COMMON_UTILTY_H_
#define SQK_COMMON_UTILTY_H_

#ifndef likely
    #define likely(x) __builtin_expect((x), 1)
#endif
#ifndef unlikely
    #define unlikely(x) __builtin_expect((x), 0)
#endif

#ifdef __GNUC__
    #define prefetch(a) __builtin_prefetch((a), 0, 1)
#else
    #define prefetch(a) ((void)a)
#endif

#endif // !SQK_COMMON_UTILTY_H_

