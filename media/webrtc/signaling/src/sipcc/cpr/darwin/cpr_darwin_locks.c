/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include <errno.h>
#include <pthread.h>

/**
 * cprCreateMutex
 *
 * @brief Creates a mutual exclusion block
 *
 * The cprCreateMutex function is called to allow the OS to perform whatever
 * work is needed to create a mutex.
 *
 * @param[in] name  - name of the mutex. If present, CPR assigns this name to
 * the mutex to assist in debugging.
 *
 * @return Mutex handle or NULL if creation failed. If NULL, set errno
 */
cprMutex_t
cprCreateMutex (const char *name)
{
    static const char fname[] = "cprCreateMutex";
    static uint16_t id = 0;
    int32_t returnCode;
    cpr_mutex_t *cprMutexPtr;
    pthread_mutex_t *linuxMutexPtr;

    /*
     * Malloc memory for a new mutex. CPR has its' own
     * set of mutexes so malloc one for the generic
     * CPR view and one for the CNU specific version.
     */
    cprMutexPtr = (cpr_mutex_t *) cpr_malloc(sizeof(cpr_mutex_t));
    linuxMutexPtr = (pthread_mutex_t *) cpr_malloc(sizeof(pthread_mutex_t));
    if ((cprMutexPtr != NULL) && (linuxMutexPtr != NULL)) {
        /* Assign name */
        cprMutexPtr->name = name;

        /*
         * Use default mutex attributes. TBD: if we do not
         * need cnuMutexAttributes global get rid of it
         */
        returnCode = pthread_mutex_init(linuxMutexPtr, NULL);
        if (returnCode != 0) {
            CPR_ERROR("%s - Failure trying to init Mutex %s: %d\n",
                      fname, name, returnCode);
            cpr_free(linuxMutexPtr);
            cpr_free(cprMutexPtr);
            return (cprMutex_t)NULL;
        }

        /*
         * TODO - It would be nice for CPR to keep a linked
         * list of active mutexes for debugging purposes
         * such as a show command or walking the list to ensure
         * that an application does not attempt to create
         * the same mutex twice.
         */
        cprMutexPtr->u.handlePtr = linuxMutexPtr;
        cprMutexPtr->lockId = ++id;
        return (cprMutex_t)cprMutexPtr;
    }

    /*
     * Since the code malloced two pointers ensure both
     * are freed since one malloc call could have worked
     * and the other failed.
     */
    if (linuxMutexPtr != NULL) {
        cpr_free(linuxMutexPtr);
    } else if (cprMutexPtr != NULL) {
        cpr_free(cprMutexPtr);
    }

    /* Malloc failed */
    CPR_ERROR("%s - Malloc for mutex %s failed.\n", fname, name);
    errno = ENOMEM;
    return (cprMutex_t)NULL;
}


/**
 * cprDestroyMutex
 *
 * @brief Destroys the mutex passed in.
 *
 * The cprDestroyMutex function is called to destroy a mutex. It is the
 * application's responsibility to ensure that the mutex is unlocked when
 * destroyed. Unpredictiable behavior will occur if an application
 * destroys a locked mutex.
 *
 * @param[in] mutex - mutex to destroy
 *
 * @return CPR_SUCCESS or CPR_FAILURE. errno should be set for CPR_FAILURE.
 */
cprRC_t
cprDestroyMutex (cprMutex_t mutex)
{
    static const char fname[] = "cprDestroyMutex";
    cpr_mutex_t *cprMutexPtr;
    int32_t rc;

    cprMutexPtr = (cpr_mutex_t *) mutex;
    if (cprMutexPtr != NULL) {
        rc = pthread_mutex_destroy(cprMutexPtr->u.handlePtr);
        if (rc != 0) {
            CPR_ERROR("%s - Failure destroying Mutex %s: %d\n",
                      fname, cprMutexPtr->name, rc);
            return CPR_FAILURE;
        }
        cprMutexPtr->lockId = 0;
        cpr_free(cprMutexPtr->u.handlePtr);
        cpr_free(cprMutexPtr);
        return CPR_SUCCESS;
    }

    /* Bad application! */
    CPR_ERROR("%s - NULL pointer passed in.\n", fname);
    errno = EINVAL;
    return CPR_FAILURE;
}


/**
 * cprGetMutex
 *
 * @brief Acquire ownership of a mutex
 *
 * This function locks the mutex referenced by the mutex parameter. If the mutex
 * is locked by another thread, the calling thread will block until the mutex is
 * released.
 *
 * @param[in] mutex - Which mutex to acquire
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprGetMutex (cprMutex_t mutex)
{
    static const char fname[] = "cprGetMutex";
    cpr_mutex_t *cprMutexPtr;
    int32_t rc;

    cprMutexPtr = (cpr_mutex_t *) mutex;
    if (cprMutexPtr != NULL) {
        rc = pthread_mutex_lock((pthread_mutex_t *) cprMutexPtr->u.handlePtr);
        if (rc != 0) {
            CPR_ERROR("%s - Error acquiring mutex %s: %d\n",
                      fname, cprMutexPtr->name, rc);
            return CPR_FAILURE;
        }
        return CPR_SUCCESS;
    }

    /* Bad application! */
    CPR_ERROR("%s - NULL pointer passed in.\n", fname);
    errno = EINVAL;
    return CPR_FAILURE;
}


/**
 * cprReleaseMutex
 *
 * @brief Release ownership of a mutex
 *
 * This function unlocks the mutex referenced by the mutex parameter.
 * @param[in] mutex - Which mutex to release
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprReleaseMutex (cprMutex_t mutex)
{
    static const char fname[] = "cprReleaseMutex";
    cpr_mutex_t *cprMutexPtr;
    int32_t rc;

    cprMutexPtr = (cpr_mutex_t *) mutex;
    if (cprMutexPtr != NULL) {
        rc = pthread_mutex_unlock((pthread_mutex_t *) cprMutexPtr->u.handlePtr);
        if (rc != 0) {
            CPR_ERROR("%s - Error releasing mutex %s: %d\n",
                      fname, cprMutexPtr->name, rc);
            return CPR_FAILURE;
        }
        return CPR_SUCCESS;
    }

    /* Bad application! */
    CPR_ERROR("%s - NULL pointer passed in.\n", fname);
    errno = EINVAL;
    return CPR_FAILURE;
}

