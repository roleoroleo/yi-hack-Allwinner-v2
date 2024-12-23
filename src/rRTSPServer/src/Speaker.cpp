/*
 * Copyright (c) 2024 roleo.
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
    int c;

    while (!s->stopThread()) {
        c = s->getCounter();
        usleep(100 * 1000);
        if (c == 0) {
            s->switchSpeaker(SPEAKER_OFF);
            s->setCounter(-1);
        } else if (c > 0) {
            s->setCounter(s->getCounter() - 100);
        }
    }

    return NULL;
}

Speaker::Speaker(sem_t *semSpeaker): fSemSpeaker(semSpeaker),
    fIsActive(False), fExitThread(False), fSpeakerCounter(-1) {

    fSemFile = strDup(SEM_FILE);
}

Speaker* Speaker::createNew() {
    int pth_ret;
    pthread_t speakerThread;
    sem_t *semSpeaker;

    // Open semaphore
    semSpeaker = sem_open(SEM_FILE, O_CREAT, 0644, 1);
    if (semSpeaker == SEM_FAILED) {
        fprintf(stderr, "Error opening %s\n", SEM_FILE);
        return NULL;
    }

    Speaker *tmpSpeaker = new Speaker(semSpeaker);

    // Start speaker thread
    pth_ret = pthread_create(&speakerThread, NULL, speaker, (void*) tmpSpeaker);
    if (pth_ret != 0) {
        fprintf(stderr, "Failed to create speaker thread\n");
        delete tmpSpeaker;
        return NULL;
    }
    pthread_detach(speakerThread);

    return tmpSpeaker;
}

Speaker::~Speaker() {
    fExitThread = True;
    usleep(100000);
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
    return fSpeakerCounter;
}

void Speaker::setCounter(int value) {
    fSpeakerCounter = value;
}

Boolean Speaker::stopThread() {
    return fExitThread;
}

int Speaker::switchSpeaker(int on)
{
    int num = -1;

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
        return -3;
    }

    if ((fIsActive == False) && (on == SPEAKER_ON)) {
        // If semaphore is locked, exit
        if (sem_trywait(fSemSpeaker) == -1) {
            fprintf(stderr, "Speaker is busy\n");
            return -1;
        }
        fIsActive = True;
        setCounter(SPEAKER_MAX_VALUE);
        if (debug) fprintf(stderr, "Speaker on\n");
    } else if (fIsActive == True) {
        if (on == SPEAKER_OFF) {
            setCounter(-1);
            fIsActive = False;
            sem_post(fSemSpeaker);
            if (debug) fprintf(stderr, "Speaker off\n");
        } else {
            setCounter(SPEAKER_MAX_VALUE);
        }
    } else {
        closeCpld(fd);
        return -4;
    }

    if (debug) fprintf(stderr, "Running ioctl: num %d\n", num);
    runIO(fd, num);

    closeCpld(fd);

    return 0;
}
