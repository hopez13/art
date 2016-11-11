/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

public class RacyLoader extends DefiningLoader {
    private Object lock = new Object();
    private int index = 0;
    private int count;

    private DefiningLoader[] defining_loaders;

    public RacyLoader(ClassLoader parent, int count) {
        super(parent);
        this.count = count;
        defining_loaders = new DefiningLoader[2];
        for (int i = 0; i != defining_loaders.length; ++i) {
            defining_loaders[i] = new DefiningLoader(parent);
        }
    }

    protected Class<?> findClass(String name) throws ClassNotFoundException
    {
        if (name.equals("Helper1") || name.equals("Helper2")) {
            return super.findClass(name);
        } else if (name.equals("Test")) {
            throw new Error("Unexpected RacyLoader.findClass(\"Test\")");
        }
        return super.findClass(name);
    }

    protected Class<?> loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        if (name.equals("Helper1") || name.equals("Helper2")) {
            return super.loadClass(name, resolve);
        } else if (name.equals("Test")) {
            int my_index = syncWithOtherInstances(count);
            Class<?> result = defining_loaders[my_index & 1].loadClass(name, resolve);
            syncWithOtherInstances(2 * count);
            return result;
        }
        return super.loadClass(name, resolve);
    }

    private int syncWithOtherInstances(int limit) {
        int my_index;
        synchronized (lock) {
            my_index = index;
            ++index;
            if (index != limit) {
                try {
                    lock.wait();
                } catch (InterruptedException ie) {
                    throw new Error(ie);
                }
            } else {
                lock.notifyAll();
            }
        }
        return my_index;
    }
}
