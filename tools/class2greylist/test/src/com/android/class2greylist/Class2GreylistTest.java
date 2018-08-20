package com.android.class2greylist;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.mockito.Mock;

import java.io.IOException;
import java.util.Map;

public class Class2GreylistTest {

    @Mock
    Status mStatus;
    @Rule
    public TestName mTestName = new TestName();

    @Before
    public void setup() throws IOException {
        System.out.println(String.format("\n============== STARTING TEST: %s ==============\n",
                mTestName.getMethodName()));
        initMocks(this);
    }

    @Test
    public void testReadGreylistMap() throws IOException {
        Map<Integer, String> map = Class2Greylist.readGreylistMap(mStatus,
                new String[]{"noApi", "1:apiOne", "3:apiThree"});
        verifyZeroInteractions(mStatus);
        assertThat(map).containsExactly(null, "noApi", 1, "apiOne", 3, "apiThree");
    }

    @Test
    public void testReadGreylistMapDuplicate() throws IOException {
        Class2Greylist.readGreylistMap(mStatus,
                new String[]{"noApi", "1:apiOne", "1:anotherOne"});
        verify(mStatus, atLeastOnce()).error(any(), any());
    }

    @Test
    public void testReadGreylistMapDuplicateNoApi() {
        Class2Greylist.readGreylistMap(mStatus,
                new String[]{"noApi", "anotherNoApi", "1:apiOne"});
        verify(mStatus, atLeastOnce()).error(any(), any());
    }

    @Test
    public void testReadGreylistMapInvalidInt() throws IOException {
        Class2Greylist.readGreylistMap(mStatus, new String[]{"noApi", "a:apiOne"});
        verify(mStatus, atLeastOnce()).error(any(), any());
    }

    @Test
    public void testReadGreylistMapNoFilename() throws IOException {
        Class2Greylist.readGreylistMap(mStatus, new String[]{"noApi", "1:"});
        verify(mStatus, atLeastOnce()).error(any(), any());
    }
}

