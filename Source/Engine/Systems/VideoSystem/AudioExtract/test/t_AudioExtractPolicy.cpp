#include <gtest/gtest.h>

#include "Engine/Systems/VideoSystem/AudioExtract/include/AudioExtract.h"

TEST(t_AudioExtractPolicy, CompanionPathSwapsExtensionToOgg)
{
    EXPECT_EQ(MakeCompanionOggPath("Assets/rezero.mp4"),       "Assets/rezero.ogg");
    EXPECT_EQ(MakeCompanionOggPath("Assets/Video/test.mp4"),   "Assets/Video/test.ogg");
    EXPECT_EQ(MakeCompanionOggPath("Assets/clip.gif"),         "Assets/clip.ogg");
}

TEST(t_AudioExtractPolicy, RegenerateWhenMissing)
{
    // oggExists == false → always regenerate, regardless of mtimes.
    EXPECT_TRUE(ShouldRegenerateCompanion(false, 0, 0));
    EXPECT_TRUE(ShouldRegenerateCompanion(false, 999, 1));
}

TEST(t_AudioExtractPolicy, RegenerateWhenOggOlderThanVideo)
{
    EXPECT_TRUE (ShouldRegenerateCompanion(true, 100, 200));  // ogg older → regenerate
    EXPECT_FALSE(ShouldRegenerateCompanion(true, 200, 100));  // ogg newer → keep
    EXPECT_FALSE(ShouldRegenerateCompanion(true, 100, 100));  // equal → keep
}
