//
//  RSFunctional.c
//  RSCoreFoundation
//
//  Created by closure on 1/29/14.
//  Copyright (c) 2014 RetVal. All rights reserved.
//

#include <RSCoreFoundation/RSFunctional.h>
#include <RSCoreFoundation/RSRuntime.h>

#pragma mark -
#pragma mark Map API Group

static RSArrayRef __RSMapArray(RSArrayRef coll, RSTypeRef (^fn)(RSTypeRef obj)) {
    RSMutableArrayRef result = RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
    RSArrayApplyBlock(coll, RSMakeRange(0, RSArrayGetCount(coll)), ^(const void *value, RSUInteger idx, BOOL *isStop) {
        RSTypeRef obj = fn(value);
        RSArrayAddObject(result, obj);
//        RSRelease(obj);
    });
    return RSAutorelease(result);
}

static RSListRef __RSReverseList(RSListRef list);
static RSListRef __RSMapList(RSListRef coll, RSTypeRef (^fn)(RSTypeRef obj)) {
    __block RSMutableArrayRef array = RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
    RSListApplyBlock(coll, ^(RSTypeRef node) {
        RSArrayAddObject(array, fn(node));
    });
    RSListRef rst = RSAutorelease(RSListCreateWithArray(RSAllocatorSystemDefault, array));
    RSRelease(array);
    return rst;
}

static RSSetRef __RSMapSet(RSSetRef coll, RSTypeRef (^fn)(RSTypeRef obj)) {
    RSMutableSetRef result = RSSetCreateMutable(RSAllocatorSystemDefault, RSSetGetCount(coll), &RSTypeSetCallBacks);
    RSSetApplyBlock(coll, ^(const void *value, BOOL *stop) {
        RSSetAddValue(result, fn(value));
    });
    return RSAutorelease(result);
}

RSExport RSCollectionRef RSMap(RSCollectionRef coll, RSTypeRef (^fn)(RSTypeRef obj)) {
    if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSArray"))) return __RSMapArray(coll, fn);
    else if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSList"))) return __RSReverseList(__RSMapList(coll, fn));
    return nil;
}

#pragma mark -
#pragma mark Reduce API Group

static RSTypeRef __RSReduceArray(RSArrayRef coll, RSTypeRef (^fn)(RSTypeRef a, RSTypeRef b)) {
    __block RSTypeRef result = RSArrayObjectAtIndex(coll, 0);
    if (RSArrayGetCount(coll) == 1)
        return RSAutorelease(fn(result, nil));
    else if (RSArrayGetCount(coll) == 2) {
        return RSAutorelease(fn(result, RSArrayObjectAtIndex(coll, 1)));
    }
    
    RSAutoreleaseBlock(^{
        RSArrayApplyBlock(coll, RSMakeRange(1, RSArrayGetCount(coll) - 1), ^(const void *value, RSUInteger idx, BOOL *isStop) {
            result = RSAutorelease(fn(result, value));
        });
        RSRetain(result);
    });
    return RSAutorelease(result);
}

static RSTypeRef __RSReduceList(RSListRef coll, RSTypeRef (^fn)(RSTypeRef a, RSTypeRef b)) {
    __block RSTypeRef result = RSFirst(coll);
    __block RSCollectionRef cache = coll;
    RSAutoreleaseBlock(^{
        while ((cache = RSNext(cache))) {
            result = fn(result, RSFirst(cache));
        }
        RSRetain(result);
    });
    return RSAutorelease(result);
}

RSExport RSTypeRef RSReduce(RSCollectionRef coll, RSTypeRef (^fn)(RSTypeRef a, RSTypeRef b)) {
    if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSArray"))) return __RSReduceArray(coll, fn);
    else if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSList"))) return __RSReduceList(coll, fn);
    return nil;
}

#pragma mark -
#pragma mark Reverse API Group

static RSArrayRef __RSReverseArray(RSArrayRef array) {
    RSUInteger cnt = RSArrayGetCount(array);
    RSMutableArrayRef coll = RSArrayCreateMutable(RSAllocatorSystemDefault, cnt);
    for (RSUInteger idx = 0; idx < cnt; idx++) {
        RSArrayAddObject(coll, RSArrayObjectAtIndex(array, cnt - idx - 1));
    }
    return RSAutorelease(coll);
}

static RSListRef __RSReverseList(RSListRef coll) {
    __block RSCollectionRef rst = RSAutorelease(RSListCreate(RSAllocatorSystemDefault, RSFirst(coll), nil));
    __block RSCollectionRef cache = coll;
    RSAutoreleaseBlock(^{
        while ((cache = RSNext(cache))) {
            rst = RSConjoin(rst, RSFirst(cache));
        }
        RSRetain(rst);
    });
    return (RSListRef)RSAutorelease(rst);
}

RSExport RSCollectionRef RSReverse(RSCollectionRef coll) {
    if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSArray"))) return __RSReverseArray(coll);
    else if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSList"))) return __RSReverseList(coll);
    return coll;
}

#pragma mark -
#pragma mark Filter API Group

static RSArrayRef __RSFilterArray(RSArrayRef coll, BOOL (^pred)(RSTypeRef x)) {
    RSMutableArrayRef rst = RSArrayCreateMutable(RSAllocatorSystemDefault, RSArrayGetCount(coll));
    RSArrayApplyBlock(coll, RSMakeRange(0, RSArrayGetCount(coll)), ^(const void *value, RSUInteger idx, BOOL *isStop) {
        if (pred(value) == YES)
            RSArrayAddObject(rst, value);
    });
    return RSAutorelease(rst);
}

static RSListRef __RSFilterList(RSListRef coll, BOOL (^pred)(RSTypeRef x)) {
    RSMutableArrayRef array = RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
    RSListApplyBlock(coll, ^(RSTypeRef value) {
        if (pred(value))
            RSArrayAddObject(array, value);
    });
    RSListRef rst = RSAutorelease(RSListCreateWithArray(RSAllocatorSystemDefault, array));
    RSRelease(array);
    return rst;
}

RSExport RSCollectionRef RSFilter(RSCollectionRef coll, BOOL (^pred)(RSTypeRef x)) {
    if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSArray"))) return __RSFilterArray(coll, pred);
    else if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSList"))) return __RSFilterList(coll, pred);
    return nil;
}

#pragma mark -
#pragma mark Drop API Group

static RSArrayRef __RSDropArray(RSArrayRef coll, RSIndex n) {
    if (!coll || RSArrayGetCount(coll) < n) return nil;
    else if (RSArrayGetCount(coll) == n) return RSAutorelease(RSArrayCreate(RSAllocatorSystemDefault, NULL));
    RSUInteger cnt = RSArrayGetCount(coll) - n;
    RSMutableArrayRef rst = RSArrayCreateMutable(RSAllocatorSystemDefault, cnt);
    for (RSUInteger idx = 0; idx < cnt; idx++) {
        RSArrayAddObject(rst, RSArrayObjectAtIndex(coll, n + idx));
    }
    return RSAutorelease(rst);
}

static RSListRef __RSDropList(RSListRef coll, RSIndex n) {
    RSListRef rst = RSListCreateDrop(RSAllocatorSystemDefault, coll, n);
    return RSAutorelease(rst);
}

RSExport RSCollectionRef RSDrop(RSCollectionRef coll, RSIndex n) {
    if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSArray"))) return __RSDropArray(coll, n);
    else if (RSInstanceIsMemberOfClass(coll, RSClassGetWithUTF8String("RSList"))) return __RSDropList(coll, n);
    return nil;
}

#pragma mark -
#pragma mark Merge API Group

static RSDictionaryRef __RSMergeDictionary(RSArrayRef colls) {
    if (!colls || !RSArrayGetCount(colls)) return nil;
    RSMutableDictionaryRef merge = RSDictionaryCreateMutable(RSAllocatorSystemDefault, 0, RSDictionaryRSTypeContext);
    RSArrayApplyBlock(colls, RSMakeRange(0, RSArrayGetCount(colls)), ^(const void *value, RSUInteger idx, BOOL *isStop) {
        RSDictionaryApplyBlock(value, ^(const void *key, const void *value, BOOL *stop) {
            RSDictionarySetValue(merge, key, value);
        });
    });
    return RSAutorelease(merge);
}

static RSArrayRef __RSMergeArray(RSArrayRef colls) {
    if (!colls || !RSArrayGetCount(colls)) return nil;
    RSMutableArrayRef merge = RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
    RSArrayApplyBlock(colls, RSMakeRange(0, RSArrayGetCount(colls)), ^(const void *value, RSUInteger idx, BOOL *isStop) {
        RSArrayAddObjects(merge, value);
    });
    return RSAutorelease(merge);
}

static RSListRef __RSMergeList(RSArrayRef colls) {
    if (!colls || !RSArrayGetCount(colls)) return nil;
    RSListRef merge = nil;
    RSMutableArrayRef buf = RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
    RSArrayApplyBlock(colls, RSMakeRange(0, RSArrayGetCount(colls)), ^(const void *value, RSUInteger idx, BOOL *isStop) {
        RSListApplyBlock(value, ^(RSTypeRef value) {
            RSArrayAddObject(buf, value);
        });
    });
    merge = RSListCreateWithArray(RSAllocatorSystemDefault, buf);
    RSRelease(merge);
    return RSAutorelease(merge);
}

RSExport RSCollectionRef RSMerge(RSCollectionRef a, RSCollectionRef b, ...) {
    if (!a || !b) return nil;
    va_list ap;
    va_start(ap, b);
    RSArrayRef collections = __RSArrayCreateWithArguments(ap, -1);
    va_end(ap);
    if (!collections) {
        collections = (RSArrayRef)RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
        RSArrayAddObjects((RSMutableArrayRef)collections, a);
        RSArrayAddObjects((RSMutableArrayRef)collections, b);
    } else {
        RSArrayInsertObjectAtIndex((RSMutableArrayRef)collections, 0, b);
        RSArrayInsertObjectAtIndex((RSMutableArrayRef)collections, 0, a);
    }
    __block RSCollectionRef merge = nil;
    RSAutoreleaseBlock(^{
        RSClassRef cls = RSClassFromInstance(RSArrayObjectAtIndex(collections, 0));
        RSArrayRef validCollections = RSFilter(collections, ^BOOL(RSTypeRef x) {
            return RSInstanceIsMemberOfClass(x, cls);
        });
        if (RSArrayGetCount(validCollections) == RSArrayGetCount(collections)) {
            if (RSInstanceIsMemberOfClass(RSArrayObjectAtIndex(validCollections, 0), RSClassGetWithUTF8String("RSDictionary")))
                merge = RSRetain(__RSMergeDictionary(validCollections));
            else if (RSInstanceIsMemberOfClass(RSArrayObjectAtIndex(validCollections, 0), RSClassGetWithUTF8String("RSArray")))
                merge = RSRetain(__RSMergeArray(validCollections));
            else if (RSInstanceIsMemberOfClass(RSArrayObjectAtIndex(validCollections, 0), RSClassGetWithUTF8String("RSList")))
                merge = RSRetain(__RSMergeList(validCollections));
        }
        RSRelease(collections);
    });
    return RSAutorelease(merge);
}

static RSClassRef listClass = nil, arrayClass = nil, dictionaryClass = nil, setClass = nil, bagClass = nil, stringClass = nil;
RSExport RSIndex RSCollPred(RSTypeRef coll) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        listClass = RSClassGetWithUTF8String("RSList"), arrayClass = RSClassGetWithUTF8String("RSArray"), dictionaryClass = RSClassGetWithUTF8String("RSDictionary"), setClass = RSClassGetWithUTF8String("RSSet"), bagClass = RSClassGetWithUTF8String("RSBag"), stringClass = RSClassGetWithUTF8String("RSString");
    });
    return RSInstanceIsMemberOfClass(coll, arrayClass) || RSInstanceIsMemberOfClass(coll, dictionaryClass) || RSInstanceIsMemberOfClass(coll, setClass) || RSInstanceIsMemberOfClass(coll, listClass) || RSInstanceIsMemberOfClass(coll, bagClass);
}

RSExport RSIndex RSCount(RSTypeRef coll) {
    if (!coll) return 0;
    if (RSCollPred(coll)) {
        if (RSInstanceIsMemberOfClass(coll, arrayClass)) return RSArrayGetCount(coll);
        else if (RSInstanceIsMemberOfClass(coll, dictionaryClass)) return RSDictionaryGetCount(coll);
        else if (RSInstanceIsMemberOfClass(coll, setClass)) return RSSetGetCount(coll);
        else if (RSInstanceIsMemberOfClass(coll, listClass)) return RSListGetCount(coll);
        else if (RSInstanceIsMemberOfClass(coll, bagClass)) return RSBagGetCount(coll);
    } else if (RSInstanceIsMemberOfClass(coll, stringClass)) {
        return RSStringGetLength(coll);
    }
    return 0;
}

