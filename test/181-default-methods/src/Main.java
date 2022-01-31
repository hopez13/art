/*
 * Copyright (C) 2022 The Android Open Source Project
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

public class Main {
    public static void expectDefault(Abstract target) {
        System.out.print("Output from " + target.getClass().getName() + ": ");
        target.testMethod();
    }

    public static void expectConflict(Abstract target) {
        try {
            target.testMethod();
            throw new Error("Unexpected success for " + target.getClass().getName());
        } catch (AbstractMethodError ame) {
            throw new Error("Unexpected AbstractMethodError" ,ame);
        } catch (IncompatibleClassChangeError expected) {
            System.out.println("Conflict in class " + target.getClass().getName());
        }
    }

    public static void expectMiranda(Abstract target) {
        try {
            target.testMethod();
            throw new Error("Unexpected success for " + target.getClass().getName());
        } catch (AbstractMethodError expected) {
            System.out.println("Miranda in class " + target.getClass().getName());
        }
    }

    public static void main(String args[]) {
        // Basic tests that have the interfaces D<n> with default method and/or
        // D<n>M interfaces that mask the default method ordered by <n>.
        expectMiranda(new Miranda());
        expectDefault(new Default_D1());
        expectDefault(new Default_D2());
        expectDefault(new Default_D3());
        expectMiranda(new Miranda_D1M());
        expectMiranda(new Miranda_D2M());
        expectMiranda(new Miranda_D3M());
        expectConflict(new Conflict_D1_D2());
        expectDefault(new Default_D1_D2M());
        expectDefault(new Default_D1M_D2());
        expectMiranda(new Miranda_D1M_D2M());
        expectConflict(new Conflict_D1_D2_D3());
        expectConflict(new Conflict_D1_D2_D3M());
        expectConflict(new Conflict_D1_D2M_D3());
        expectDefault(new Default_D1_D2M_D3M());
        expectConflict(new Conflict_D1M_D2_D3());
        expectDefault(new Default_D1M_D2_D3M());
        expectDefault(new Default_D1M_D2M_D3());
        expectMiranda(new Miranda_D1M_D2M_D3M());

        // Cases where one interface masks the method in more than one superinterface.
        expectMiranda(new Miranda_D1D2M());
        expectDefault(new Default_D1D2D());

        // Cases where the interface D2 is early in the interface table but masked by D2M later.
        expectDefault(new Default_D2_D1_D2M());
        expectMiranda(new Miranda_D2_D1M_D2M());

        // Cases that involve a superclass with miranda method in the vtable.
        expectDefault(new Default_D1M_x_D2());
        expectMiranda(new Miranda_D1M_x_D2M());
        expectConflict(new Conflict_D1M_x_D2_D3());
        expectDefault(new Default_D1M_x_D2_D3M());
        expectDefault(new Default_D1M_x_D2M_D3());
        expectMiranda(new Miranda_D1M_x_D2M_D3M());

        // Cases that involve a superclass with default method in the vtable.
        expectConflict(new Conflict_D1_x_D2());
        expectDefault(new Default_D1_x_D2M());
        expectDefault(new Default_D1_x_D1MD());
        expectMiranda(new Miranda_D1_x_D1M());
        expectConflict(new Conflict_D1_x_D2_D3());
        expectConflict(new Conflict_D1_x_D2_D3M());
        expectConflict(new Conflict_D1_x_D2M_D3());
        expectDefault(new Default_D1_x_D2M_D3M());
        expectConflict(new Conflict_D1_x_D1MD_D2());
        expectDefault(new Default_D1_x_D1MD_D2M());
        expectDefault(new Default_D1_x_D1M_D2());
        expectMiranda(new Miranda_D1_x_D1M_D2M());

        // Cases that involve a superclass with conflict method in the vtable.
        expectDefault(new Default_D1_D2_x_D2M());
        expectDefault(new Default_D1_D2_x_D1M());
    }
}

class Miranda extends Base implements Abstract {}
class Default_D1 extends Base implements Abstract, D1 {}
class Default_D2 extends Base implements Abstract, D2 {}
class Default_D3 extends Base implements Abstract, D3 {}
class Miranda_D1M extends Base implements Abstract, D1M {}
class Miranda_D2M extends Base implements Abstract, D2M {}
class Miranda_D3M extends Base implements Abstract, D3M {}
class Conflict_D1_D2 extends Base implements Abstract, D1, D2 {}
class Default_D1_D2M extends Base implements Abstract, D1, D2M {}
class Default_D1M_D2 extends Base implements Abstract, D1M, D2 {}
class Miranda_D1M_D2M extends Base implements Abstract, D1M, D2M {}
class Conflict_D1_D2_D3 extends Base implements Abstract, D1, D2, D3 {}
class Conflict_D1_D2_D3M extends Base implements Abstract, D1, D2, D3M {}
class Conflict_D1_D2M_D3 extends Base implements Abstract, D1, D2M, D3 {}
class Default_D1_D2M_D3M extends Base implements Abstract, D1, D2M, D3M {}
class Conflict_D1M_D2_D3 extends Base implements Abstract, D1M, D2, D3 {}
class Default_D1M_D2_D3M extends Base implements Abstract, D1M, D2, D3M {}
class Default_D1M_D2M_D3 extends Base implements Abstract, D1M, D2M, D3 {}
class Miranda_D1M_D2M_D3M extends Base implements Abstract, D1M, D2M, D3M {}

class Miranda_D1D2M extends Base implements D1D2M {}
class Default_D1D2D extends Base implements D1D2D {}

class Default_D2_D1_D2M extends Base implements Abstract, D1, D2M {}
class Miranda_D2_D1M_D2M extends Base implements Abstract, D1M, D2M {}

class Default_D1M_x_D2 extends Miranda_D1M implements D2 {}
class Miranda_D1M_x_D2M extends Miranda_D1M implements D2M {}
class Conflict_D1M_x_D2_D3 extends Miranda_D1M implements D2, D3 {}
class Default_D1M_x_D2_D3M extends Miranda_D1M implements D2, D3M {}
class Default_D1M_x_D2M_D3 extends Miranda_D1M implements D2M, D3 {}
class Miranda_D1M_x_D2M_D3M extends Miranda_D1M implements D2M, D3M {}

class Conflict_D1_x_D2 extends Default_D1 implements D2 {}
class Default_D1_x_D2M extends Default_D1 implements D2M {}
class Default_D1_x_D1MD extends Default_D1 implements D1MD {}
class Miranda_D1_x_D1M extends Default_D1 implements D1M {}
class Conflict_D1_x_D2_D3 extends Default_D1 implements D2, D3 {}
class Conflict_D1_x_D2_D3M extends Default_D1 implements D2, D3M {}
class Conflict_D1_x_D2M_D3 extends Default_D1 implements D2M, D3 {}
class Default_D1_x_D2M_D3M extends Default_D1 implements D2M, D3M {}
class Conflict_D1_x_D1MD_D2 extends Default_D1 implements D1MD, D2 {}
class Default_D1_x_D1MD_D2M extends Default_D1 implements D1MD, D2M {}
class Default_D1_x_D1M_D2 extends Default_D1 implements D1M, D2 {}
class Miranda_D1_x_D1M_D2M extends Default_D1 implements D1M, D2M {}

class Default_D1_D2_x_D2M extends Conflict_D1_D2 implements D2M {}
class Default_D1_D2_x_D1M extends Conflict_D1_D2 implements D1M {}

