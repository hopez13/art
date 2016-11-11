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

public class WrapperLoader extends FancyLoader {
    private FancyLoader fancy_loader;

    public WrapperLoader(ClassLoader parent, FancyLoader fancy_loader) {
        super(parent);
        this.fancy_loader = fancy_loader;
    }

    public void resetFancyLoader(FancyLoader fancy_loader) {
        this.fancy_loader = fancy_loader;
    }

    protected Class<?> findClass(String name) throws ClassNotFoundException
    {
        if (name.equals("Helper")) {
            return super.findClass(name);
        } else if (name.equals("Test")) {
            return fancy_loader.findClass(name);
        }
        return super.findClass(name);
    }

    protected Class<?> loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        if (name.equals("Helper")) {
            return super.loadClass(name, resolve);
        } else if (name.equals("Test")) {
            return fancy_loader.loadClass(name, resolve);
        }
        return super.loadClass(name, resolve);
    }
}
