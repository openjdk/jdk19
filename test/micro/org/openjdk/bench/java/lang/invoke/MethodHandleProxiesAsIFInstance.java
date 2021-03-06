/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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
package org.openjdk.bench.java.lang.invoke;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleProxies;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.util.concurrent.TimeUnit;

/**
 * Benchmark evaluates the performance of MethodHandleProxies.*
 */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@State(Scope.Thread)
@Warmup(iterations = 10, time = 1, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 5, time = 2, timeUnit = TimeUnit.SECONDS)
@Fork(3)
public class MethodHandleProxiesAsIFInstance {

    /**
     * Implementation notes:
     *   - asInterfaceInstance() can only target static MethodHandle (adapters needed to call instance method?)
     *   - baselineCompute will quickly degrade to GC test, if escape analysis is unable to spare the allocation
     *   - testCreate* will always be slower if allocation is not eliminated; baselineAllocCompute makes sure allocation is present
     */

    public int i;

    private MethodHandle target;
    private Doable precreated;

    @Setup
    public void setup() throws Throwable {
        target = MethodHandles.lookup().findStatic(MethodHandleProxiesAsIFInstance.class, "doWork", MethodType.methodType(int.class, int.class));
        precreated = MethodHandleProxies.asInterfaceInstance(Doable.class, target);
    }

    @Benchmark
    public Doable testCreate() {
        Doable doable = MethodHandleProxies.asInterfaceInstance(Doable.class, target);
        return doable;              // make sure allocation happens
    }

    @Benchmark
    public Doable testCreateCall() {
        Doable doable = MethodHandleProxies.asInterfaceInstance(Doable.class, target);
        i = doable.doWork(i);       // make sure computation happens
        return null;                // let allocation be eliminated
    }

    @Benchmark
    public Doable testCall() {
        i = precreated.doWork(i);   // make sure computation happens
        return precreated;
    }

    @Benchmark
    public Doable baselineCompute() {
        Doable doable = new Doable() {
            @Override
            public int doWork(int i) {
                return MethodHandleProxiesAsIFInstance.doWork(i);
            }
        };

        i = doable.doWork(i);       // make sure computation happens
        return null;                // let allocation be eliminated
    }

    @Benchmark
    public Doable baselineAllocCompute() {
        Doable doable = new Doable() {
            @Override
            public int doWork(int i) {
                return MethodHandleProxiesAsIFInstance.doWork(i);
            }
        };

        i = doable.doWork(i);       // make sure computation happens
        return doable;              // make sure allocation happens
    }

    public static int doWork(int i) {
        return i + 1;
    }

    public interface Doable {
        int doWork(int i);
    }

}
