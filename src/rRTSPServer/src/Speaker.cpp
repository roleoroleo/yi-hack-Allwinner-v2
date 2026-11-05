/*
 * Copyright (c) 2026 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Class to handle GPIOS to activate speaker
 */

#include "Boolean.hh"
#include "strDup.hh"
#include "Speaker.hh"

#include <cstdio>

#include "unistd.h"
#include "sys/ioctl.h"

extern int debug;

static void *speaker(void *ptr)
{
    Speaker *s = (Speaker *) ptr;

    while (!s->stopThread()) {
        usleep(100 * 1000);
        s->tick(100);
    }

    return NULL;
}

// Count down the time since the last audio packet and switch the speaker
// off when it expires. Check and switch happen under the same lock, so a
// packet arriving meanwhile can't be cut off by a stale decision.
void Speaker::tick(int elapsedMs) {
    pthread_mutex_lock(&fLock);
    if (fSpeakerCounter > 0) {
        fSpeakerCounter -= elapsedMs;
        if (fSpeakerCounter <= 0) {
            switchSpeakerLocked(SPEAKER_OFF);
        }
    }
    pthread_mutex_unlock(&fLock);
}

Speaker::Speaker(sem_t *semSpeaker): fSemSpeaker(semSpeaker),
    fIsActive(False), fExitThread(False), fThread(0), fSpeakerCounter(-1) {

    pthread_mutex_init(&fLock, NULL);
    fSemFile = strDup(SEM_FILE);
}

Speaker* Speaker::createNew() {
    int pth_ret;
    sem_t *semSpeaker;

    // Open semaphore
    semSpeaker = sem_open(SEM_FILE, O_CREAT, 0644, 1);
    if (semSpeaker == SEM_FAILED) {
        fprintf(stderr, "Error opening %s\n", SEM_FILE);
        return NULL;
    }

    Speaker *tmpSpeaker = new Speaker(semSpeaker);

    // Start speaker thread
    pth_ret = pthread_create(&tmpSpeaker->fThread, NULL, speaker, (void*) tmpSpeaker);
    if (pth_ret != 0) {
        fprintf(stderr, "Failed to create speaker thread\n");
        tmpSpeaker->fThread = 0;
        delete tmpSpeaker;
        return NULL;
    }

    return tmpSpeaker;
}

Speaker::~Speaker() {
    fExitThread = True;
    // Wait for the speaker thread to finish before the object goes away
    if (fThread != 0) pthread_join(fThread, NULL);

    // If the session ends while the speaker is on, switch it off and
    // release the semaphore, otherwise it stays locked for everyone
    pthread_mutex_lock(&fLock);
    if (fIsActive) switchSpeakerLocked(SPEAKER_OFF);
    pthread_mutex_unlock(&fLock);
    pthread_mutex_destroy(&fLock);

    if (fSemSpeaker != SEM_FAILED) {
        sem_close(fSemSpeaker);
        fSemSpeaker = SEM_FAILED;
    }
    delete[] (char*) fSemFile;
}

int Speaker::openCpld() {
    int fd;

    if (access(CPLD_DEV, F_OK) == 0) {
        fd = open(CPLD_DEV, O_RDWR);
    } else {
        fd = -1;
    }

    return fd;
}

void Speaker::closeCpld(int fd) {
    close(fd);
}

void Speaker::runIO(int fd, int n) {
    ioctl(fd, _IOC(0, DEVICE_NUM, n, 0x00), 0);
}

Boolean Speaker::isActive() {
    return fIsActive;
}

int Speaker::getCounter() {
    pthread_mutex_lock(&fLock);
    int value = fSpeakerCounter;
    pthread_mutex_unlock(&fLock);
    return value;
}

void Speaker::setCounter(int value) {
    pthread_mutex_lock(&fLock);
    fSpeakerCounter = value;
    pthread_mutex_unlock(&fLock);
}

Boolean Speaker::stopThread() {
    return fExitThread;
}

int Speaker::switchSpeaker(int on)
{
    pthread_mutex_lock(&fLock);
    int ret = switchSpeakerLocked(on);
    pthread_mutex_unlock(&fLock);
    return ret;
}

int Speaker::switchSpeakerLocked(int on)
{
    int num = -1;
    int ret = 0;

    if (on == SPEAKER_ON) {
        num = 16;
    } else if (on == SPEAKER_OFF) {
        num = 17;
    }

    if (num == -1) {
        fprintf(stderr, "Error: wrong parameter %d\n", on);
        return -2;
    }

    int fd = openCpld();
    if (fd < 0) {
        fprintf(stderr, "Error: cannot open %s\n", CPLD_DEV);
        if ((on == SPEAKER_OFF) && (fIsActive == True)) {
            // Can't drive the GPIO, but still release our state and the semaphore
            fSpeakerCounter = -1;
            fIsActive = False;
            sem_post(fSemSpeaker);
        }
        return -3;
    }

    if ((fIsActive == False) && (on == SPEAKER_ON)) {
        // If semaphore is locked, exit
        if (sem_trywait(fSemSpeaker) == -1) {
            fprintf(stderr, "Speaker is busy\n");
            ret = -1;
        } else {
            fIsActive = True;
            fSpeakerCounter = SPEAKER_MAX_VALUE;
            if (debug) fprintf(stderr, "Speaker on\n");
        }
    } else if (fIsActive == True) {
        if (on == SPEAKER_OFF) {
            fSpeakerCounter = -1;
            fIsActive = False;
            sem_post(fSemSpeaker);
            if (debug) fprintf(stderr, "Speaker off\n");
        } else {
            fSpeakerCounter = SPEAKER_MAX_VALUE;
        }
    } else {
        ret = -4;
    }

    if (ret == 0) {
        if (debug) fprintf(stderr, "Running ioctl: num %d\n", num);
        runIO(fd, num);
    }

    // Close the device on every path (it leaked when the speaker was busy)
    closeCpld(fd);

    return ret;
}
