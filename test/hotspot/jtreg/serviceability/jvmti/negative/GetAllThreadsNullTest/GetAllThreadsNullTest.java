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
 * @summary converted from VM Testbase nsk/jvmti/GetAllThreads/allthr002.
 * VM Testbase keywords: [quick, jpda, jvmti, noras]
 * VM Testbase readme:
 * DESCRIPTION
 *     The test exercises JVMTI function GetAllThreads(threadsCountPtr, threadsPtr)
 *     The test checks the following:
 *       - if JVMTI_ERROR_NULL_POINTER is returned when threadsCountPtr is null
 *       - if JVMTI_ERROR_NULL_POINTER is returned when threadsPtr is null
 * COMMENTS
 *     Ported from JVMDI.
 *
 * @library /test/lib
 * @run main/othervm/native -agentlib:GetAllThreadsNullTest GetAllThreadsNullTest
 */

public class GetAllThreadsNullTest {

    static {
        try {
            System.loadLibrary("GetAllThreadsNullTest");
        } catch (UnsatisfiedLinkError ule) {
            System.err.println("Could not load GetAllThreadsNullTest library");
            System.err.println("java.library.path:"
                + System.getProperty("java.library.path"));
            throw ule;
        }
    }

    native static boolean check();

    public static void main(String args[]) {
        if (!check()) {
            throw new RuntimeException("GetAllThreads doesn't fail if one of parameter is NULL.");
        }
    }

}
