#include "TEST_UTILS/TestUtils.h"
#include "../../SOURCE/Util/FileUtils.h"

TEST_CASE("FileUtils::isSupportedAudioFile", "[FileUtils]")
{
    SECTION("WAV files are supported")
    {
        juce::File wavFile("test.wav");
        CHECK(FileUtils::isSupportedAudioFile(wavFile) == true);

        juce::File upperWav("TEST.WAV");
        CHECK(FileUtils::isSupportedAudioFile(upperWav) == true);
    }

    SECTION("MP3 files are supported")
    {
        juce::File mp3File("test.mp3");
        CHECK(FileUtils::isSupportedAudioFile(mp3File) == true);

        juce::File upperMp3("TEST.MP3");
        CHECK(FileUtils::isSupportedAudioFile(upperMp3) == true);
    }

    SECTION("Other file types are not supported")
    {
        juce::File txtFile("test.txt");
        CHECK(FileUtils::isSupportedAudioFile(txtFile) == false);

        juce::File flacFile("test.flac");
        CHECK(FileUtils::isSupportedAudioFile(flacFile) == false);

        juce::File noExtension("test");
        CHECK(FileUtils::isSupportedAudioFile(noExtension) == false);
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

        CHECK(result == true);
        CHECK(errorMsg.isEmpty() == true);
    }

    SECTION("Non-existent file fails validation")
    {
        juce::File nonExistent("this_file_does_not_exist.wav");
        juce::String errorMsg;

        bool result = FileUtils::validateInputFile(nonExistent, errorMsg);

        CHECK(result == false);
        CHECK(errorMsg.isNotEmpty() == true);
        CHECK((errorMsg.contains("not found") || errorMsg.contains("does not exist")));
    }

    SECTION("Unsupported file type fails validation")
    {
        // Create a temporary text file
        auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("test_file.txt");
        tempFile.create();

        juce::String errorMsg;
        bool result = FileUtils::validateInputFile(tempFile, errorMsg);

        CHECK(result == false);
        CHECK(errorMsg.isNotEmpty() == true);
        CHECK((errorMsg.contains("not supported") || errorMsg.contains("format")));

        // Cleanup
        tempFile.deleteFile();
    }

    SECTION("Directory instead of file fails validation")
    {
        auto dir = juce::File::getCurrentWorkingDirectory();
        juce::String errorMsg;

        bool result = FileUtils::validateInputFile(dir, errorMsg);

        CHECK(result == false);
        CHECK(errorMsg.isNotEmpty() == true);
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

        CHECK(result == true);
        CHECK(errorMsg.isEmpty() == true);
    }

    SECTION("Valid MP3 output path passes validation")
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto outputFile = tempDir.getChildFile("test_output.mp3");

        juce::String errorMsg;
        bool result = FileUtils::validateOutputPath(outputFile, errorMsg);

        CHECK(result == true);
        CHECK(errorMsg.isEmpty() == true);
    }

    SECTION("Unsupported extension fails validation")
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto outputFile = tempDir.getChildFile("test_output.txt");

        juce::String errorMsg;
        bool result = FileUtils::validateOutputPath(outputFile, errorMsg);

        CHECK(result == false);
        CHECK(errorMsg.isNotEmpty() == true);
        CHECK((errorMsg.contains("not supported") || errorMsg.contains("extension")));
    }

    SECTION("Parent directory must exist")
    {
        auto nonExistentDir = juce::File("C:/this/path/does/not/exist/output.wav");
        juce::String errorMsg;

        bool result = FileUtils::validateOutputPath(nonExistentDir, errorMsg);

        CHECK(result == false);
        CHECK(errorMsg.isNotEmpty() == true);
    }

    SECTION("Empty filename fails validation")
    {
        juce::File emptyFile;
        juce::String errorMsg;

        bool result = FileUtils::validateOutputPath(emptyFile, errorMsg);

        CHECK(result == false);
        CHECK(errorMsg.isNotEmpty() == true);
    }
}
