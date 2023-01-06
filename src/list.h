#ifndef GIT_LIVE_LIST_H
#define GIT_LIVE_LIST_H

#define LIST_APPEND(head, elm, field) \
do {                                           \
    if (LIST_EMPTY(head)) {                    \
        LIST_INSERT_HEAD(head, elm, field);    \
    } else {                                   \
        typeof(elm) __last = LIST_FIRST(head); \
        while (LIST_NEXT(__last, field)) {     \
            __last = LIST_NEXT(__last, field); \
        }                                      \
        LIST_INSERT_AFTER(__last, elm, field); \
    }                                          \
} while(0)

#define NODES_FOREACH(var, head) LIST_FOREACH(var, head, entry)

#define NODES_FOREACH_N(var, num, node, n) 			\
	for ((num) = 0, (var) = (node);				    \
		(var) && (num) < (n);						\
		(var) = LIST_NEXT((var), entry), (num)++)



#endif //GIT_LIVE_LIST_H
