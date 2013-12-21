//
//  RSHTTPCookieStorage.c
//  RSCoreFoundation
//
//  Created by closure on 12/17/13.
//  Copyright (c) 2013 RetVal. All rights reserved.
//

#include "RSHTTPCookieStorage.h"

#include <RSCoreFoundation/RSRuntime.h>
#include <RSCoreFoundation/RSArchiver.h>
#include "RSPrivate/RSPrivateOperatingSystem/RSPrivateFileSystem.h"

RS_CONST_STRING_DECL(__RSHTTPCookieStoragePreferencePathTargetFormat, "~/Library/Cookies/RSCoreFoundation/%s/RSCookie.plist");
RS_CONST_STRING_DECL(__RSHTTPCookieStoragePreferencePathTargetRSFormat, "~/Library/Cookies/RSCoreFoundation/%r/RSCookie.plist");
RS_CONST_STRING_DECL(__RSHTTPCookieStoragePreferencePathDirectoryFormat, "~/Library/Cookies/RSCoreFoundation/%s/");
RS_CONST_STRING_DECL(__RSHTTPCookieStoragePreferencePathDirectoryRSFormat, "~/Library/Cookies/RSCoreFoundation/%s/");


RS_CONST_STRING_DECL(__RSHTTPCookieStorageBasePathKey, "BasePath");             //  RSString


// cache
RS_CONST_STRING_DECL(__RSHTTPCookieStorageTransformation, "Transformation");    //  RSArray - RSDictionary - {HOST - UUID }
RS_CONST_STRING_DECL(__RSHTTPCookieStorageUUID, "UUID");                        //  RSArray - RSArray - (cookie - properties)


struct __RSHTTPCookieStorage {
    RSRuntimeBase _base;
    RSMutableDictionaryRef _cache;
    RSStringRef _storageBasePath;
    RSDictionaryRef _preferences;
};

static RSStringRef __RSHTTPCookieStorageDefaultFilePath() {
    const char *name = nil;
    RSBundleRef bundle = RSBundleGetMainBundle();
    if (!bundle) {
        name = *_RSGetProgname();
    }
    RSStringRef path = RSStringCreateWithFormat(RSAllocatorSystemDefault, name ? __RSHTTPCookieStoragePreferencePathTargetFormat : __RSHTTPCookieStoragePreferencePathTargetRSFormat, (name ? (RSStringRef)name : RSBundleGetIdentifier(bundle)));
    RSStringRef returnPath = RSFileManagerStandardizingPath(path);
    RSRelease(path);
    return returnPath;
}

static RSStringRef __RSHTTPCookieStorageDefaultDirectoryPath() {
    const char *name = nil;
    RSBundleRef bundle = RSBundleGetMainBundle();
    if (!bundle) {
        name = *_RSGetProgname();
    }
    RSStringRef path = RSStringCreateWithFormat(RSAllocatorSystemDefault, name ? __RSHTTPCookieStoragePreferencePathDirectoryFormat : __RSHTTPCookieStoragePreferencePathDirectoryRSFormat, (name ? (RSStringRef)name : RSBundleGetIdentifier(bundle)));
    RSStringRef returnPath = RSFileManagerStandardizingPath(path);
    RSRelease(path);
    return returnPath;
}

static RSStringRef __RSHTTPCookieStorageBasePath(RSUUIDRef uuid) {
    RSStringRef dir = __RSHTTPCookieStorageDefaultDirectoryPath();
    RSStringRef path = RSStringWithFormat(RSSTR("%r%r"), dir, RSAutorelease(RSUUIDCreateString(RSAllocatorSystemDefault, uuid)));
    return path;
}

static BOOL __RSHTTPCookieStorageCacheInit(RSStringRef path) {
    RSDictionaryRef cache = RSDictionaryCreateWithObjectsAndOKeys(RSAllocatorSystemDefault, RSAutorelease(RSDictionaryCreateMutable(RSAllocatorSystemDefault, 0, RSDictionaryRSTypeContext)), __RSHTTPCookieStorageTransformation, RSAutorelease(RSDictionaryCreateMutable(RSAllocatorSystemDefault, 0, RSDictionaryRSTypeContext)), __RSHTTPCookieStorageUUID, NULL);
//    BOOL result = RSDictionaryWriteToFile(cache, path, RSWriteFileAutomatically);
    
    RSArchiverRef archiver = RSArchiverCreate(RSAllocatorSystemDefault);
    RSArchiverEncodeObjectForKey(archiver, RSKeyedArchiveRootObjectKey, cache);
    RSRelease(cache);
    
    RSDataRef data = RSArchiverCopyData(archiver);
    RSRelease(archiver);
    
    BOOL result = RSDataWriteToFile(data, path, RSWriteFileAutomatically);
    RSRelease(data);
    return result;
}

static BOOL __RSHTTPCookieStoragePreferencesInit(RSStringRef path) {
    RSStringRef targetDirectory = __RSHTTPCookieStorageDefaultDirectoryPath();
    RSStringRef targetFile = __RSHTTPCookieStorageDefaultFilePath();
    RSFileManagerCreateDirectoryAtPath(RSFileManagerGetDefault(), targetDirectory);
    
    RSStringRef basePath = path ? : __RSHTTPCookieStorageBasePath(RSAutorelease(RSUUIDCreate(RSAllocatorSystemDefault)));
    if (__RSHTTPCookieStorageCacheInit(basePath)) {
        RSDictionaryRef preferences = RSDictionaryCreateWithObjectsAndOKeys(RSAllocatorSystemDefault, basePath, __RSHTTPCookieStorageBasePathKey, nil);
        BOOL result = RSDictionaryWriteToFile(preferences, targetFile, RSWriteFileAutomatically);
        RSRelease(preferences);
        return result;
    }
    return NO;
}

static void __RSHTTPCookieStorageClassInit(RSTypeRef rs) {
    RSHTTPCookieStorageRef storage = (RSHTTPCookieStorageRef)rs;
    BOOL isFile = NO;
    RSStringRef targetFile = __RSHTTPCookieStorageDefaultFilePath();
    if (!(RSFileManagerFileExistsAtPath(RSFileManagerGetDefault(), targetFile, &isFile) && !isFile)) {
        if (!__RSHTTPCookieStoragePreferencesInit(nil)) {
            __RSCLog(RSLogLevelWarning, "%s %s\n", __func__, "can not init default settings.");
            return;
        }
    }
    storage->_preferences = RSDictionaryCreateWithContentOfPath(RSAllocatorSystemDefault, __RSHTTPCookieStorageDefaultFilePath());
    storage->_storageBasePath = RSRetain(RSDictionaryGetValue(storage->_preferences, __RSHTTPCookieStorageBasePathKey));
    if (!RSFileManagerFileExistsAtPath(RSFileManagerGetDefault(), storage->_storageBasePath, &isFile)) {
        __RSHTTPCookieStoragePreferencesInit(storage->_storageBasePath);
    }
    RSUnarchiverRef unarchiver = RSUnarchiverCreateWithContentOfPath(RSAllocatorSystemDefault, storage->_storageBasePath);
    RSTypeRef rootObject = RSUnarchiverDecodeObjectForKey(unarchiver, RSKeyedArchiveRootObjectKey);
    RSRelease(unarchiver);
    storage->_cache = RSMutableCopy(RSAllocatorSystemDefault, rootObject);
    RSRelease(rootObject);
}

static void __RSHTTPCookieStorageClassDeallocate(RSTypeRef rs) {
    RSHTTPCookieStorageRef storage = (RSHTTPCookieStorageRef)rs;
    RSArchiverRef archiver = RSArchiverCreate(RSAllocatorSystemDefault);
    RSArchiverEncodeObjectForKey(archiver, RSKeyedArchiveRootObjectKey, storage->_cache);
    RSDataRef data = RSArchiverCopyData(archiver);
    RSRelease(archiver);
    RSDataWriteToFile(data, storage->_storageBasePath, RSWriteFileAutomatically);
    RSRelease(data);
    RSRelease(storage->_preferences);
    RSRelease(storage->_storageBasePath);
    RSRelease(storage->_cache);
}

static RSHashCode __RSHTTPCookieStorageClassHash(RSTypeRef rs) {
    return RSHash(((RSHTTPCookieStorageRef)rs)->_storageBasePath);
}

static RSStringRef __RSHTTPCookieStorageClassDescription(RSTypeRef rs) {
    RSStringRef description = RSStringCreateWithFormat(RSAllocatorDefault, RSSTR("RSHTTPCookieStorage %p"), rs);
    return description;
}

static RSRuntimeClass __RSHTTPCookieStorageClass =
{
    _RSRuntimeScannedObject,
    "RSHTTPCookieStorage",
    __RSHTTPCookieStorageClassInit,
    nil,
    __RSHTTPCookieStorageClassDeallocate,
    nil,
    __RSHTTPCookieStorageClassHash,
    __RSHTTPCookieStorageClassDescription,
    nil,
    nil
};

static RSDataRef __RSHTTPCookieStorageSerializeCallback(RSArchiverRef archiver, RSTypeRef object) {
    RSHTTPCookieStorageRef storage = (RSHTTPCookieStorageRef)object;
    RSDataRef data = RSArchiverEncodeObject(archiver, storage->_cache);
    return data;
}

static RSTypeRef __RSHTTPCookieStorageDeserializeCallback(RSUnarchiverRef unarchiver, RSDataRef data) {
    RSDictionaryRef dict = RSUnarchiverDecodeObject(unarchiver, data);
    return dict;
}

static RSSpinLock __RSHTTPCookieSharedStorageLock = RSSpinLockInit;
static RSHTTPCookieStorageRef __RSHTTPCookieSharedStorage = nil;

static RSHTTPCookieStorageRef __RSHTTPCookieStorageCreateInstance(RSAllocatorRef allocator);
static void __RSHTTPCookieStorageDeallocate(RSNotificationRef notification);

static RSTypeID _RSHTTPCookieStorageTypeID = _RSRuntimeNotATypeID;

RSExport RSTypeID RSHTTPCookieStorageGetTypeID() {
    if (!__RSHTTPCookieSharedStorageLock)
        RSHTTPCookieStorageGetSharedStorage();
    return _RSHTTPCookieStorageTypeID;
}

static void __RSHTTPCookieStorageInitialize() {
    _RSHTTPCookieStorageTypeID = __RSRuntimeRegisterClass(&__RSHTTPCookieStorageClass);
    __RSRuntimeSetClassTypeID(&__RSHTTPCookieStorageClass, _RSHTTPCookieStorageTypeID);
    
    const struct __RSArchiverCallBacks __RSHTTPCookieStorageArchiverCallbacks = {
        0,
        _RSHTTPCookieStorageTypeID,
        __RSHTTPCookieStorageSerializeCallback,
        __RSHTTPCookieStorageDeserializeCallback
    };
    if (!RSArchiverRegisterCallbacks(&__RSHTTPCookieStorageArchiverCallbacks))
        __RSCLog(RSLogLevelWarning, "%s %s\n", __func__, "fail to register archiver routine");
    RSNotificationCenterAddObserver(RSNotificationCenterGetDefault(), RSAutorelease(RSObserverCreate(RSAllocatorSystemDefault, RSCoreFoundationWillDeallocateNotification, &__RSHTTPCookieStorageDeallocate, nil)));
}

static void __RSHTTPCookieStorageDeallocate(RSNotificationRef notification) {
    RSSyncUpdateBlock(__RSHTTPCookieSharedStorageLock, ^{
        if (nil != __RSHTTPCookieSharedStorage) {
            __RSRuntimeSetInstanceSpecial(__RSHTTPCookieSharedStorage, NO);
            RSRelease(__RSHTTPCookieSharedStorage);
            __RSHTTPCookieSharedStorage = nil;
        }
    });
}

static RSHTTPCookieStorageRef __RSHTTPCookieStorageCreateInstance(RSAllocatorRef allocator)
{
    RSHTTPCookieStorageRef instance = (RSHTTPCookieStorageRef)__RSRuntimeCreateInstance(allocator, _RSHTTPCookieStorageTypeID, sizeof(struct __RSHTTPCookieStorage) - sizeof(RSRuntimeBase));
    
    return instance;
}

RSExport RSHTTPCookieStorageRef RSHTTPCookieStorageGetSharedStorage() {
    RSSyncUpdateBlock(__RSHTTPCookieSharedStorageLock, ^{
        if (nil == __RSHTTPCookieSharedStorage) {
            __RSHTTPCookieStorageInitialize();
            __RSHTTPCookieSharedStorage = __RSHTTPCookieStorageCreateInstance(RSAllocatorSystemDefault);
            __RSRuntimeSetInstanceSpecial(__RSHTTPCookieSharedStorage, YES);
        }
    });
    return __RSHTTPCookieSharedStorage;
}

static RSStringRef __RSHTTPCookieStorageUpdateCookieInCache(RSDictionaryRef cache, RSHTTPCookieRef cookie) {
    RSMutableDictionaryRef transformation = (RSMutableDictionaryRef)RSDictionaryGetValue(cache, __RSHTTPCookieStorageTransformation);
    if (!transformation) {
        RSMutableDictionaryRef dict = RSDictionaryCreateMutable(RSAllocatorSystemDefault, 0, RSDictionaryRSTypeContext);
        RSDictionarySetValue((RSMutableDictionaryRef)cache, __RSHTTPCookieStorageTransformation, dict);
        RSRelease(dict);
        transformation = dict;
    }
    RSDictionaryRef paths = RSDictionaryGetValue(transformation, RSHTTPCookieGetDomain(cookie));
    if (!paths) {
        RSMutableDictionaryRef dict = RSDictionaryCreateMutable(RSAllocatorSystemDefault, 0, RSDictionaryRSTypeContext);
        RSDictionarySetValue((RSMutableDictionaryRef)transformation, RSHTTPCookieGetDomain(cookie), dict);
        RSRelease(dict);
        paths = dict;
    }
    RSMutableArrayRef uuids = (RSMutableArrayRef)RSDictionaryGetValue(paths, RSHTTPCookieGetPath(cookie));
    if (!(uuids && RSArrayGetCount(uuids))) {
        RSMutableArrayRef array = RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
        RSDictionarySetValue((RSMutableDictionaryRef)paths, RSHTTPCookieGetPath(cookie), array);
        RSRelease(array);
        uuids = array;
    }
    __block RSStringRef result = nil;
    RSArrayApplyBlock(uuids, RSMakeRange(0, RSArrayGetCount(uuids)), ^(const void *value, RSUInteger idx, BOOL *isStop) {
        *isStop = RSEqual(cookie, RSDictionaryGetValue(RSDictionaryGetValue(cache, __RSHTTPCookieStorageUUID), value));
        if (*isStop) result = value;
    });
    if (!result) {
        result = RSUUIDCreateString(RSAllocatorSystemDefault, RSAutorelease(RSUUIDCreate(RSAllocatorSystemDefault)));
        RSArrayAddObject(uuids, result);
        RSRelease(result);
    }
    RSDictionarySetValue((RSMutableDictionaryRef)RSDictionaryGetValue(cache, __RSHTTPCookieStorageUUID), result, cookie);
    
    return result;
}

static RSStringRef __RSHTTPCookieStorageFindCookieInCache(RSDictionaryRef cache, RSHTTPCookieRef cookie) {
    /*
        ----cache(dict)
        |
        |----Transformation(array)
        |            |
        |            |----Host Names, Paths (UUIDs)
        |
        |
        |----UUID----Cookie
     */
    RSDictionaryRef transformation = RSDictionaryGetValue(cache, __RSHTTPCookieStorageTransformation);
    if (!transformation) {
        RSMutableDictionaryRef dict = RSDictionaryCreateMutable(RSAllocatorSystemDefault, 0, RSDictionaryRSTypeContext);
        RSDictionarySetValue((RSMutableDictionaryRef)cache, __RSHTTPCookieStorageTransformation, dict);
        RSRelease(dict);
        return nil;
    }
    RSDictionaryRef paths = RSDictionaryGetValue(transformation, RSHTTPCookieGetDomain(cookie));
    if (!paths) {
        RSMutableDictionaryRef dict = RSDictionaryCreateMutable(RSAllocatorSystemDefault, 0, RSDictionaryRSTypeContext);
        RSDictionarySetValue((RSMutableDictionaryRef)cache, RSHTTPCookieGetDomain(cookie), dict);
        RSRelease(dict);
        return nil;
    }
    RSArrayRef uuids = RSDictionaryGetValue(paths, RSHTTPCookieGetPath(cookie));
    if (!(uuids && RSArrayGetCount(uuids))) {
        RSMutableArrayRef array = RSArrayCreateMutable(RSAllocatorSystemDefault, 0);
        RSDictionarySetValue((RSMutableDictionaryRef)cache, RSHTTPCookieGetPath(cookie), array);
        RSRelease(array);
        return nil;
    }
    
    __block RSStringRef result = nil;
    RSArrayApplyBlock(uuids, RSMakeRange(0, RSArrayGetCount(uuids)), ^(const void *value, RSUInteger idx, BOOL *isStop) {
        *isStop = RSEqual(cookie, RSDictionaryGetValue(RSDictionaryGetValue(cache, __RSHTTPCookieStorageUUID), value));
        if (*isStop) result = value;
    });
    return result;
}

static RSStringRef __RSHTTPCookieStorageUpdateCookieTransformationAndUUID(RSHTTPCookieStorageRef storage, RSHTTPCookieRef cookie) {
    RSStringRef uuidString = RSRetain(__RSHTTPCookieStorageFindCookieInCache(storage->_cache, cookie));
    if (!uuidString) {
        RSUUIDRef uuid = RSUUIDCreate(RSAllocatorSystemDefault);
        uuidString = RSUUIDCreateString(RSAllocatorSystemDefault, uuid);
    }
    RSRelease(uuidString);
    return uuidString;
}

static void __RSHTTPCookieStorageUpdateCookie(RSHTTPCookieStorageRef storage, RSHTTPCookieRef cookie) {
    RSMutableArrayRef transformation = (RSMutableArrayRef)RSDictionaryGetValue(storage->_cache, __RSHTTPCookieStorageTransformation);
    RSMutableArrayRef uuids = (RSMutableArrayRef)RSDictionaryGetValue(storage->_cache, __RSHTTPCookieStorageUUID);
}

RSExport void RSHTTPCookieStorageSetCookie(RSHTTPCookieStorageRef storage, RSHTTPCookieRef cookie) {
    if (!storage || !cookie) return;
    __RSGenericValidInstance(storage, _RSHTTPCookieStorageTypeID);
    RSSyncUpdateBlock(__RSHTTPCookieSharedStorageLock, ^{
        __RSHTTPCookieStorageUpdateCookieInCache(storage->_cache, cookie);
    });
}