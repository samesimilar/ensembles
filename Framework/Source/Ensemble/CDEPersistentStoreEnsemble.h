//
//  CDEPersistentStoreEnsemble.h
//  Ensembles
//
//  Created by Drew McCormack on 4/11/13.
//  Copyright (c) 2013 Drew McCormack. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import "CDEDefines.h"


@class CDEPersistentStoreEnsemble;
@protocol CDECloudFileSystem;

/**
 @name Notifications
 */

/**
 Posted when ensembles observes that a `NSManagedObjectContext` will save to the monitored persistent store. You can monitor this notification rather than the standard `NSManagedObjectContextWillSaveNotification` if you want to be sure that the ensemble has already prepared for the save when the notification is observed. If you observe `NSManagedObjectContextWillSaveNotification` directly, you can't be sure that the ensemble has observed the notification, because order of receivers is not defined.
 
 The object for the notification is not the ensemble, but the context that is saving. The ensemble observing the save is accessible in the `userInfo` dictionary via the key `persistentStoreEnsemble`.
 */
extern NSString * const CDEMonitoredManagedObjectContextWillSaveNotification;

/**
 Posted when the ensemble observes that a `NSManagedObjectContext` has saved to the monitored persistent store. You can monitor this notification rather than the standard `NSManagedObjectContexDidSaveNotification` if you want to be sure that the ensemble has already processed the save when the notification is observed. If you observe `NSManagedObjectContextDidSaveNotification` directly, you can't be sure that the ensemble has observed the notification, because order of receivers is not defined.
 
  The object for the notification is not the ensemble, but the context that is saving. The ensemble observing the save is accessible in the `userInfo` dictionary via the key `persistentStoreEnsemble`.
 */
extern NSString * const CDEMonitoredManagedObjectContextDidSaveNotification;

/**
 This notification is fired after the ensemble has merged changes and performed a background save into the persistent store. You can use this notification to invoke the `mergeChangesFromContextDidSaveNotification`: method on any of the contexts that depend on the content of the store. Alternatively, you can implement the `persistentStoreEnsemble`:didSaveMergeChangesWithNotification: method for this purpose.
 
 The object for the notification is the ensemble. The save notification, which is what is passed to the `mergeChangesFromContextDidSaveNotification`: method, is provided in the `userInfo` dictionary with the key `CDEManagedObjectContextSaveNotificationKey`.
 
 @warning This notification is posted on the background thread where the merge save occurred. It is important to invoke the `mergeChangesFromContextDidSaveNotification`: method on the thread/queue corresponding to the `NSManagedObjectContext` merging the changes.
 */
extern NSString * const CDEPersistentStoreEnsembleDidSaveMergeChangesNotification;

/**
 Used as a key in the `userInfo` dictionary of the `CDEPersistentStoreEnsembleDidSaveMergeChangesNotification` notification. It's value is the original notification resulting from the save, and can be passed to the `mergeChangesFromContextDidSaveNotification`: method to update other contexts that access the persistent store.
 */
extern NSString * const CDEManagedObjectContextSaveNotificationKey;


/**
 @name Delegate
 */

/**
 A protocol that includes methods invoked by the `CDEPeristentStoreEnsemble`. The ensemble uses this to inform of sync-related changes.
 */
@protocol CDEPersistentStoreEnsembleDelegate <NSObject>

@optional

/**
 @brief Invoked during leeching when the contents of the persistent store are about to be migrated to the cloud.
 
 @param ensemble The `CDEPersistentStoreEnsemble` that is about to import
 */
- (void)persistentStoreEnsembleWillImportStore:(CDEPersistentStoreEnsemble *)ensemble;

/**
 @brief Invoked during leeching when the contents of the persistent store have been migrated to the cloud.
 
 @param ensemble The `CDEPersistentStoreEnsemble` that is importing the store
 */
- (void)persistentStoreEnsembleDidImportStore:(CDEPersistentStoreEnsemble *)ensemble;

/**
 @brief Invoked when the ensemble is about to attempt to save merged changes into the persistent store.
 
 This method is invoked on a background thread. Both of the contexts passed have private queue concurrency type, and so they should only be accessed via calls to `performBlock...` methods.
 
 You can use the saving context to check what changes have been made in the merge via `NSManagedObjectContext` methods like `insertedObjects`, `updatedObjects`, and `deletedObjects`.
 
 You should not make any changes directly in the saving context. If you need to make changes before the save is attempted, you can make them in the reparation context.
 
 You can force the merge to terminate altogether by returning `NO` from this method.
 
 @param ensemble The `CDEPersistentStoreEnsemble` that will attempt to save
 @param savingContext A private-queue context which includes the unsaved changes that will be committed to the store
 @param reparationContext A private-queue context that can be used to make any changes necessary to allow the save to succeed
 @return YES if the save should be attempted, and NO to abort the merge entirely
 @warning Be careful not to nest calls to the `performBlock...` methods for the two contexts. This will very likely lead to a deadlock, because the contexts in question have a parent-child relationship.
 */
- (BOOL)persistentStoreEnsemble:(CDEPersistentStoreEnsemble *)ensemble shouldSaveMergedChangesInManagedObjectContext:(NSManagedObjectContext *)savingContext reparationManagedObjectContext:(NSManagedObjectContext *)reparationContext;

/**
 @brief Invoked when the ensemble attempted to save merged changes into the persistent store, but the save failed.
 
 This method is invoked on a background thread. Both of the contexts passed have private queue concurrency type, and so they should only be accessed via calls to `performBlock...` methods.
 
 You can use the saving context to check what changes failed to save via `NSManagedObjectContext` methods like `insertedObjects`, `updatedObjects`, and `deletedObjects`. The error that occurred during saving is passed and can be used to determine which objects are responsible for the failure.
 
 You should not make any changes directly in the saving context. If you wish to reattempt the save, make any necessary changes in the reparation context, and then return `YES`.
 
 You can force the merge to terminate altogether by returning `NO` from this method.
 
 @param ensemble The `CDEPersistentStoreEnsemble` that attempted the save
 @param savingContext A private-queue context which includes the unsaved changes that will be committed to the store
 @param error The error returned by the `save`: method
 @param reparationContext A private-queue context that can be used to make any changes necessary to allow the save to be reattempted
 @return YES if the save should be reattempted, and NO to abort the merge entirely
 @warning Be careful not to nest calls to the `performBlock...` methods for the two contexts. This will very likely lead to a deadlock, because the contexts in question have a parent-child relationship.
 */
- (BOOL)persistentStoreEnsemble:(CDEPersistentStoreEnsemble *)ensemble didFailToSaveMergedChangesInManagedObjectContext:(NSManagedObjectContext *)savingContext error:(NSError *)error reparationManagedObjectContext:(NSManagedObjectContext *)reparationContext;

/**
 @brief Invoked after the ensemble successfully saves merged changes into the persistent store.
 
 This method is invoked on the thread used for saving the changes. The notification passed includes the `userInfo` dictionary from the notification that was posted when the context saved. It can be used to determine what object insertions, updates, and deletions occurred.
 
 You will usually want to pass this notification to the `mergeChangesFromContextDidSaveNotification`: method of any context that accesses the persistent store, be it directly or indirectly. This will allow the context to account for the changes.
 
 @warning Be sure to invoke the `mergeChangesFromContextDidSaveNotification`: method on the thread/queue corresponding to the messaged context.
 
 @param ensemble The `CDEPersistentStoreEnsemble` that saved the changes
 @param notification A notification object containing the `userInfo` included by the saving context in the `NSManagedObjectContextDidSaveNotification` notification
 
 @see `CDEPersistentStoreEnsembleDidSaveMergeChangesNotification`
 */
- (void)persistentStoreEnsemble:(CDEPersistentStoreEnsemble *)ensemble didSaveMergeChangesWithNotification:(NSNotification *)notification;

/**
 @brief Invoked when the ensemble is forced to deleech due to an unexpected issue.
 
 In normal operation, an ensemble will not deleech unless requested to do so. However, circumstances can arise that can compromise the integrity of the sync data, in which case, the ensemble can elect to spontaneously deleech. 
 
 By way of example, if the user logs out of the cloud service used for sync, the ensemble will deleech, as it no longer has access to the cloud data.
 
 You can use this method to update the interface to reflect the deleeched state, and perhaps notify the user of the issue. You can attempt to leech again to continue syncing.
 
 @param ensemble The ensemble that is deleeching
 @param error An error describing the cause of the deleech
 */
- (void)persistentStoreEnsemble:(CDEPersistentStoreEnsemble *)ensemble didDeleechWithError:(NSError *)error;

/**
 @brief Supply global identifiers for objects.
 
 By default, the ensemble will generate random global identifiers for all objects. If logically-equivalent objects are inserted on multiple devices, after merging, there will be duplicates.
 
 You can either deduplicate these objects in code, by fetching, and deleting a duplicate, or you can instead provide global identifiers which Ensembles can use to identify corresponding objects and merge them automatically.
 
 Whenever the ensemble needs to know the global identifier of one or more objects, it will invoke this method. You can determine the identifiers as you please, but they must be immutable: an object should not have its global identifier change at any point. If you find you are tempted to change the global identifier of an object, it is better to delete the object instead, and create a new object with the new identifier.
 
 The global identifiers do not have to be stored in the persistent store, but it often works out to be the best solution. You can either determine the global identifier from existing properties (eg email, tag), or store a random identifier like a uuid.
 
 If you have certain objects in the array for which you do not wish to assign your own global identifier, you can return `NSNull` in that position.
 
 @param ensemble The ensemble requesting global identifiers
 @param objects The objects for which global identifiers are requested
 @return An array of global identifiers for the objects passed, in the same order. `NSNull` can be inserted in this array where no global identifier is needed.
 */
- (NSArray *)persistentStoreEnsemble:(CDEPersistentStoreEnsemble *)ensemble globalIdentifiersForManagedObjects:(NSArray *)objects;

@end


/**
 @brief The central class of Ensembles, it represents a set of synchronizing persistent stores.
 
 An ensemble can be seen as a set of persistent stores that exchange data in order to reach eventual consistency. The `CDEPersistentStoreEnsemble` class is responsible for monitoring saves to a persistent store, exchanging this data with ensemble objects on other peers, and merging changes from other peers into its persistent store.
 
 You typically create one `CDEPersistentStoreEnsemble` object for each persistent store that you need to sync in your app. An ensemble has an identifier, which is used to match it with other ensemble objects on peer devices. Ensemble objects with the same identifier are considered to represent corresponding persistent stores, and the data in the persistent stores will be merged.
 
 The process of initially setting up an ensemble object for communication with its peers is known as 'leeching'. An ensemble begins in a deleeched state. Leeching prepares local storage needed by the framework, registers the device in the cloud, and migrates the data in the local persistent store into the cloud. Leeching is persistent, and typically only need be performed once, though it is possible to request that the ensemble deleech.
 
 Once an ensemble is leeched, it can merge changes from other devices. Merging involves replaying changes from other devices, together with locally recorded changes, in order to update the persistent store. If changes are made concurrently on different devices, there is no guarantee that the data will be valid after replaying the changes. The ensemble provides delegate methods that can be used to make repairs to the data before committing. The changes made in reparation are also captured by the ensemble and transferred to other peers.
 */
@interface CDEPersistentStoreEnsemble : NSObject

@property (nonatomic, weak, readwrite) id <CDEPersistentStoreEnsembleDelegate> delegate;
@property (nonatomic, strong, readonly) id <CDECloudFileSystem> cloudFileSystem;
@property (nonatomic, strong, readonly) NSString *localDataRootDirectory;
@property (nonatomic, strong, readonly) NSString *ensembleIdentifier;
@property (nonatomic, strong, readonly) NSString *storePath;
@property (nonatomic, strong, readonly) NSURL *managedObjectModelURL;
@property (nonatomic, strong, readonly) NSManagedObjectModel *managedObjectModel;
@property (nonatomic, assign, readonly, getter = isLeeched) BOOL leeched;
@property (nonatomic, assign, readonly, getter = isMerging) BOOL merging;

- (instancetype)initWithEnsembleIdentifier:(NSString *)identifier persistentStorePath:(NSString *)path managedObjectModelURL:(NSURL *)modelURL cloudFileSystem:(id <CDECloudFileSystem>)newCloudFileSystem;
- (instancetype)initWithEnsembleIdentifier:(NSString *)identifier persistentStorePath:(NSString *)path managedObjectModelURL:(NSURL *)modelURL cloudFileSystem:(id <CDECloudFileSystem>)newCloudFileSystem localDataRootDirectory:(NSString *)dataRoot;

- (void)leechPersistentStoreWithCompletion:(CDECompletionBlock)completion;
- (void)deleechPersistentStoreWithCompletion:(CDECompletionBlock)completion;

- (void)mergeWithCompletion:(CDECompletionBlock)completion;
- (void)cancelMergeWithCompletion:(CDECompletionBlock)completion;

- (void)processPendingChangesWithCompletion:(CDECompletionBlock)block;

- (void)stopMonitoringSaves;

@end

@interface CDEPersistentStoreEnsemble (Internal)

- (NSArray *)globalIdentifiersForManagedObjects:(NSArray *)objects;

@end
