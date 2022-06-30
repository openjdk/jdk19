/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/**
 * @test
 * @bug 8287847
 * @summary Test suspending a thread after it has terminated.
 * @enablePreview
 * @requires vm.continuations
 * @run build TestScaffold VMConnection TargetListener TargetAdapter
 * @run compile SuspendAfterDeath.java
 * @run main/othervm SuspendAfterDeath
 */
import com.sun.jdi.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.util.*;

class SuspendAfterDeathTarg {
    static final String THREAD_NAME = "duke";

    // breakpoint here
    static void done() {
    }

    public static void main(String[] args) throws Exception {
        boolean useVirtualThread = ((args.length > 0) && args[0].equals("Virtual"));
        Thread thread;
        System.out.println("Starting debuggee " + (useVirtualThread ? "virtual" : "platform") + " thread.");
        if (useVirtualThread) {
            thread = Thread.ofVirtual().name(THREAD_NAME).start(() -> { });
        } else {
            thread = Thread.ofPlatform().name(THREAD_NAME).start(() -> { });
        }
        thread.join();
        done();
    }
}

public class SuspendAfterDeath extends TestScaffold {
    private volatile ThreadReference thread;
    private volatile boolean breakpointReached;

    SuspendAfterDeath(String args[]) {
        super(args);
    }

    public static void main(String[] args) throws Exception {
        new SuspendAfterDeath(args).startTests();
    }

    @Override
    public void threadDied(ThreadDeathEvent event) {
        ThreadReference eventThread = event.thread();
        if (eventThread.name().equals(SuspendAfterDeathTarg.THREAD_NAME)) {
            System.out.println("Target thread died, thread=" + eventThread);
            thread = eventThread;
        }
    }

    @Override
    public void breakpointReached(BreakpointEvent event) {
        ThreadReference eventThread = event.thread();
        System.out.println("Breakpoint, thread=" + eventThread);
        if (thread == null) {
            failure("FAILED: got Breakpoint event before ThreadDeath event.");
        }
        breakpointReached = true;
        /* Suspend the thread. This is being done after the thread has exited. */
        thread.suspend();
    }

    @Override
    public void connect(String args[]) {
        String mainWrapper = System.getProperty("main.wrapper");
        if ("Virtual".equals(mainWrapper)) {
            List<String> argList = new ArrayList(Arrays.asList(args));
            argList.add("Virtual");
            args = argList.toArray(args);
        }
        super.connect(args);
    }

    @Override
    protected void runTests() throws Exception {
        BreakpointEvent bpe = startToMain("SuspendAfterDeathTarg");
        EventRequestManager erm = vm().eventRequestManager();

        // listener for ThreadDeathEvent captures reference to the thread
        ThreadDeathRequest request1 = erm.createThreadDeathRequest();
        request1.enable();

        // listener for BreakpointEvent attempts to suspend the thread
        ReferenceType targetClass = bpe.location().declaringType();
        Location loc = findMethod(targetClass, "done", "()V").location();
        BreakpointRequest request2 = erm.createBreakpointRequest(loc);
        request2.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
        request2.enable();

        listenUntilVMDisconnect();

        if (thread == null) {
            failure("FAILED: never got ThreadDeath event for target thread.");
        }
        if (!breakpointReached) {
            failure("FAILED: never got Breakpoint event for target thread.");
        }

        if (!testFailed) {
            println("SuspendAfterDeath: passed");
        } else {
            throw new Exception("SuspendAfterDeath: failed");
        }
    }
}
