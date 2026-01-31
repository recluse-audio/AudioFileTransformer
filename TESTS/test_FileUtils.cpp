#include "TEST_UTILS/TestUtils.h"
#include "../../SOURCE/Util/FileUtils.h"

TEST_CASE("FileUtils::isSupportedAudioFile", "[FileUtils]")
{
    SECTION("WAV files are supported")
    {
        juce::File wavFile("test.wav");
        REQUIRE(FileUtils::isSupportedAudioFile(wavFile) == true);

        juce::File upperWav("TEST.WAV");
        REQUIRE(FileUtils::isSupportedAudioFile(upperWav) == true);
    }

    SECTION("MP3 files are supported")
    {
        juce::File mp3File("test.mp3");
        REQUIRE(FileUtils::isSupportedAudioFile(mp3File) == true);

        juce::File upperMp3("TEST.MP3");
        REQUIRE(FileUtils::isSupportedAudioFile(upperMp3) == true);
    }

    SECTION("Other file types are not supported")
    {
        juce::File txtFile("test.txt");
        REQUIRE(FileUtils::isSupportedAudioFile(txtFile) == false);

        juce::File flacFile("test.flac");
        REQUIRE(FileUtils::isSupportedAudioFile(flacFile) == false);

        juce::File noExtension("test");
        REQUIRE(FileUtils::isSupportedAudioFile(noExtension) == false);
    }
}

TEST_CASE("FileUtils::validateInputFile", "[FileUtils]")
{
    SECTION("Valid WAV file passes validation")
    {
        // Use the existing test file
        auto testFile = juce::File::getCurrentWorkingDirectory()
            .getChildFile("TESTS/TEST_FILES/Somewhere_Mono_48k.wav");

        juce::String errorMsg;
        bool result = FileUtils::validateInputFile(testFile, errorMsg);

        REQUIRE(result == true);
        REQUIRE(errorMsg.isEmpty() == true);
    }

    SECTION("Non-existent file fails validation")
    {
        juce::File nonExistent("this_file_does_not_exist.wav");
        juce::String errorMsg;

        bool result = FileUtils::validateInputFile(nonExistent, errorMsg);

        REQUIRE(result == false);
        REQUIRE(errorMsg.isNotEmpty() == true);
        REQUIRE((errorMsg.contains("not found") || errorMsg.contains("does not exist")));
    }

    SECTION("Unsupported file type fails validation")
    {
        // Create a temporary text file
        auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_file.txt");
        tempFile.create();

        juce::String errorMsg;
        bool result = FileUtils::validateInputFile(tempFile, errorMsg);

        REQUIRE(result == false);
        REQUIRE(errorMsg.isNotEmpty() == true);
        REQUIRE((errorMsg.contains("not supported") || errorMsg.contains("format")));

        // Cleanup
        tempFile.deleteFile();
    }

    SECTION("Directory instead of file fails validation")
    {
        auto dir = juce::File::getCurrentWorkingDirectory();
        juce::String errorMsg;

        bool result = FileUtils::validateInputFile(dir, errorMsg);

        REQUIRE(result == false);
        REQUIRE(errorMsg.isNotEmpty() == true);
    }
}

TEST_CASE("FileUtils::validateOutputPath", "[FileUtils]")
{
    SECTION("Valid WAV output path passes validation")
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto outputFile = tempDir.getChildFile("test_output.wav");

        juce::String errorMsg;
        bool result = FileUtils::validateOutputPath(outputFile, errorMsg);

        REQUIRE(result == true);
        REQUIRE(errorMsg.isEmpty() == true);
    }

    SECTION("Valid MP3 output path passes validation")
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto outputFile = tempDir.getChildFile("test_output.mp3");

        juce::String errorMsg;
        bool result = FileUtils::validateOutputPath(outputFile, errorMsg);

        REQUIRE(result == true);
        REQUIRE(errorMsg.isEmpty() == true);
    }

    SECTION("Unsupported extension fails validation")
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto outputFile = tempDir.getChildFile("test_output.txt");

        juce::String errorMsg;
        bool result = FileUtils::validateOutputPath(outputFile, errorMsg);

        REQUIRE(result == false);
        REQUIRE(errorMsg.isNotEmpty() == true);
        REQUIRE((errorMsg.contains("not supported") || errorMsg.contains("extension")));
    }

    SECTION("Parent directory must exist")
    {
        auto nonExistentDir = juce::File("C:/this/path/does/not/exist/output.wav");
        juce::String errorMsg;

        bool result = FileUtils::validateOutputPath(nonExistentDir, errorMsg);

        REQUIRE(result == false);
        REQUIRE(errorMsg.isNotEmpty() == true);
    }

    SECTION("Empty filename fails validation")
    {
        juce::File emptyFile;
        juce::String errorMsg;

        bool result = FileUtils::validateOutputPath(emptyFile, errorMsg);

        REQUIRE(result == false);
        REQUIRE(errorMsg.isNotEmpty() == true);
    }
}
