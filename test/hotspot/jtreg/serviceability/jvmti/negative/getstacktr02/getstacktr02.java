/*
 * Copyright (c) 2003, 2022, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 *
 * @summary converted from VM Testbase nsk/jvmti/GetStackTrace/getstacktr002.
 * VM Testbase keywords: [quick, jpda, jvmti, noras]
 * VM Testbase readme:
 * DESCRIPTION
 *     The test exercises JVMTI function
 *         GetStackTrace(thread, start_depth, max_count, stack_buffer, count_ptr).
 *     The test checks the following:
 *       - if JVMTI_ERROR_INVALID_THREAD is returned when thread is a thread object
 *       - if JVMTI_ERROR_ILLEGAL_ARGUMENT is returned when max_count is -1
 *       - if JVMTI_ERROR_NULL_POINTER is returned when stack_buffer is null
 *       - if JVMTI_ERROR_NULL_POINTER is returned when count_ptr is null
 * COMMENTS
 *     Ported from JVMDI.
 *
 * @library /test/lib
 * @run main/othervm/native -agentlib:getstacktr02 getstacktr02
 */
import java.io.PrintStream;

public class getstacktr02 {

    final static int JCK_STATUS_BASE = 95;

    static {
        try {
            System.loadLibrary("getstacktr02");
        } catch (UnsatisfiedLinkError ule) {
            System.err.println("Could not load getstacktr02 library");
            System.err.println("java.library.path:"
                + System.getProperty("java.library.path"));
            throw ule;
        }
    }

    native static int check(Thread thread);

    public static void main(String args[]) {


        // produce JCK-like exit status.
        System.exit(run(args, System.out) + JCK_STATUS_BASE);
    }

    public static int run(String args[], PrintStream out) {
        return check(Thread.currentThread());
    }
}
