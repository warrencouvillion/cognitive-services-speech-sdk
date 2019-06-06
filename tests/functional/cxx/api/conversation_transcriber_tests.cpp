//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "test_utils.h"
#include "file_utils.h"
#include "recognizer_utils.h"

#include "speechapi_cxx.h"
#include "wav_file_reader.h"

using namespace std;
using namespace Microsoft::CognitiveServices::Speech::Impl; // for CSpxWavFileReader
using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Audio;

#define SPX_CONFIG_TRACE_INTERFACE_MAP

TEST_CASE("conversation_voice_signature", "[.][int][prod]")
{
    auto audioEndpoint = Config::InroomEndpoint;
    audioEndpoint += "/multiaudio";
    auto config = SpeechConfig::FromEndpoint(audioEndpoint, Keys::ConversationTranscriber);

    katieSteve.UpdateFullFilename(Config::InputDir);
    auto audioInput = AudioConfig::FromWavFileInput(katieSteve.m_inputDataFilename);
    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);
    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);
    auto p = Participant::From("katie@example.com", "en-us");
    REQUIRE_THROWS(p->SetVoiceSignature(R"(
    {
        "Version": 0,
        "Data" : "IlgqQLfiEofDG1asXEAReulsL3GfTNF"
    })"));
    REQUIRE_THROWS(p->SetVoiceSignature(R"(
    {
        "Version": 0,
        "Tag" : "IlgqQLfiEofDG1asXEAReulsL3GfTNF"
    })"));
    REQUIRE_THROWS(p->SetVoiceSignature(R"(
    {
        "Tag": "asdfasdf",
        "Data" : "IlgqQLfiEofDG1asXEAReulsL3GfTNF"
    })"));
    REQUIRE_THROWS(Participant::From("katie@example.com", "en-us", "asdf"));
    REQUIRE_THROWS(p->SetVoiceSignature("asdf"));

    std::string katieVoiceSignature, steveVoiceSignature;
    CreateVoiceSignatures(&katieVoiceSignature, &steveVoiceSignature);

    recognizer->SetConversationId("carbon_voice_signature_test");
    REQUIRE_NOTHROW(p->SetVoiceSignature(katieVoiceSignature));
    recognizer->AddParticipant(p);

    auto p2 = Participant::From("steve@example.com", "en-us", steveVoiceSignature);
    recognizer->AddParticipant(p2);

    recognizer->StartTranscribingAsync().get();

    WaitForResult(result->ready.get_future(), 5min);
    recognizer->StopTranscribingAsync().get();

    // info is reported if any of the following requires fail, otherwise not reported.
    //https://github.com/catchorg/Catch2/blob/master/docs/logging.md
    INFO(GetText(result->phrases));
    SPXTEST_REQUIRE(VerifyTextAndSpeaker(result->phrases, "Good morning Katie.", "steve@example.com") == true);
    SPXTEST_REQUIRE(VerifyTextAndSpeaker(result->phrases, "Good morning Steve.", "katie@example.com") == true);
}

TEST_CASE("conversation_id", "[.][int][prod]")
{
    auto config = SpeechConfig::FromEndpoint(Config::InroomEndpoint, Keys::ConversationTranscriber);
    katieSteve.UpdateFullFilename(Config::InputDir);
    auto audioInput = AudioConfig::FromWavFileInput(katieSteve.m_inputDataFilename);
    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);
    auto myId = "123";
    recognizer->SetConversationId(myId);
    auto gotId = recognizer->GetConversationId();
    SPXTEST_REQUIRE(gotId == myId);

    // chinese
    unsigned char s[] = { 0xe7, 0x9a, 0x84, 0 };
    recognizer->SetConversationId(reinterpret_cast<char*>(s));
    gotId = recognizer->GetConversationId();
    SPXTEST_REQUIRE((PAL::stricmp(reinterpret_cast<char*>(s), gotId.c_str())) == 0);
}

TEST_CASE("conversation_create_users", "[.][int][prod]")
{
    auto myId = "emailOfSomeUser";
    auto user = User::FromUserId(myId);
    SPXTEST_REQUIRE(user->GetId() == myId);
}

TEST_CASE("conversation_create_participant", "[.][int][prod]")
{
    REQUIRE_THROWS(Participant::From(""));
    REQUIRE_NOTHROW(Participant::From("secondparticipant"));
    REQUIRE_NOTHROW(Participant::From("secondparticipant", "en-us"));
    REQUIRE_THROWS(Participant::From("secondparticipant", "en-us", "voiceSignature"));
}

TEST_CASE("conversation_add_while_pumping", "[.][int][prod]")
{
    auto audioEndpoint = Config::InroomEndpoint;
    audioEndpoint += "/multiaudio";
    auto config = SpeechConfig::FromEndpoint(audioEndpoint, Keys::ConversationTranscriber);

    std::shared_ptr<PullAudioInputStream> pullAudio;
    auto audioInput = CreateAudioPullFromRecordedFile(pullAudio);

    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);
    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

    recognizer->SetConversationId("AddParticipantWhileAudioPumping");
    recognizer->StartTranscribingAsync().get();
    this_thread::sleep_for(100ms);
    recognizer->AddParticipant("AddParticipantWhileAudioPumping");
    // the recorded audio is really long, intentionally timeout in 5min.
    WaitForResult(result->ready.get_future(), 5min);
    auto text = GetText(result->phrases);
    INFO(text);
    bool res = VerifyTextAndSpeaker(result->phrases, "ABC.", "Unidentified") || VerifyTextAndSpeaker(result->phrases, "ABC", "Unidentified");
    SPXTEST_REQUIRE(res == true);
}

TEST_CASE("conversation_bad_connection", "[.][int][prod]")
{
    auto audioEndpoint = "wrong_endpoint";
    auto config = SpeechConfig::FromEndpoint(audioEndpoint, Keys::ConversationTranscriber);

    std::shared_ptr<PullAudioInputStream> pullAudio;
    auto audioInput = CreateAudioPullUsingKatieSteveFile(pullAudio);

    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);

    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

    recognizer->SetConversationId("bad_connection");
    recognizer->StartTranscribingAsync().get();

    WaitForResult(result->ready.get_future(), 5min);
    recognizer->StopTranscribingAsync().get();
    SPXTEST_REQUIRE(result->phrases[0].Text == "Runtime error: Failed to create transport request.");
}

TEST_CASE("conversation_inroom_8_channel_file", "[.][int][prod]")
{
    auto audioEndpoint = Config::InroomEndpoint;
    audioEndpoint += "/multiaudio";
    auto config = SpeechConfig::FromEndpoint(audioEndpoint, Keys::ConversationTranscriber);
    config->SetProperty("FLAC", "1");

    katieSteve.UpdateFullFilename(Config::InputDir);
    auto audioInput = AudioConfig::FromWavFileInput(katieSteve.m_inputDataFilename);
    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);
    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);
    auto p = Participant::From("conversation_inroom_8_channel_file", "en-us");
    StartMeetingAndVerifyResult(recognizer.get(), p, move(result), katieSteve.m_utterance);
}
TEST_CASE("conversation_inroom_8_channel_audio_pull", "[.][int][prod]")
{
    auto audioEndpoint = Config::InroomEndpoint;
    audioEndpoint += "/multiaudio";
    auto config = SpeechConfig::FromEndpoint(audioEndpoint, Keys::ConversationTranscriber);

    std::shared_ptr<PullAudioInputStream> pullAudio;
    auto audioInput = CreateAudioPullUsingKatieSteveFile(pullAudio);

    string katieVoiceSignature, steveVoiceSignature;
    CreateVoiceSignatures(&katieVoiceSignature, &steveVoiceSignature);
    auto katie = Participant::From("katie@example.com", "en-us", katieVoiceSignature);
    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);

    SPXTEST_SECTION("AddParticipantByParticipantObj")
    {
        auto result = make_shared<RecoPhrases>();
        ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);
        StartMeetingAndVerifyResult(recognizer.get(), katie, move(result), katieSteve.m_utterance);
    }
    SPXTEST_SECTION("AddParticipantByUserId")
    {
        auto result = make_shared<RecoPhrases>();
        ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

        recognizer->SetConversationId("AddParticipantByUserIdTest");

        // add by user id
        REQUIRE_NOTHROW(recognizer->AddParticipant("AddParticipantById_1"));
        REQUIRE_NOTHROW(recognizer->AddParticipant("AddParticipantById_2"));

        // verify in the debugger that no AddParticipantById_3 in the speech.event
        REQUIRE_NOTHROW(recognizer->AddParticipant("AddParticipantById_3"));
        REQUIRE_NOTHROW(recognizer->RemoveParticipant("AddParticipantById_3"));

        REQUIRE_THROWS(recognizer->RemoveParticipant("AddParticipantById_4"));

        recognizer->StartTranscribingAsync().get();
        WaitForResult(result->ready.get_future(), 15min);
        recognizer->StopTranscribingAsync().get();

        INFO(GetText(result->phrases));
        SPXTEST_REQUIRE(!result->phrases.empty());
        SPXTEST_REQUIRE(VerifyText(result->phrases[0].Text, katieSteve.m_utterance));
    }

    SPXTEST_SECTION("AddParticipantByUserObject")
    {
        auto result = make_shared<RecoPhrases>();
        ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

        recognizer->SetConversationId("AddParticipantByUserObject");

        // create a user object
        auto user = User::FromUserId("CreateUserfromUserId");
        REQUIRE_NOTHROW(recognizer->AddParticipant(user));

        recognizer->StartTranscribingAsync().get();
        WaitForResult(result->ready.get_future(), 15min);
        recognizer->StopTranscribingAsync().get();

        INFO(GetText(result->phrases));
        SPXTEST_REQUIRE(!result->phrases.empty());
        SPXTEST_REQUIRE(VerifyText(result->phrases[0].Text, katieSteve.m_utterance));
    }
    SPXTEST_SECTION("AddParticipantSetters")
    {
        auto result = make_shared<RecoPhrases>();
        ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

        recognizer->SetConversationId("Conversation12345");

        auto p1 = Participant::From("id1");

        REQUIRE_NOTHROW(p1->SetVoiceSignature({ katieVoiceSignature }));
        REQUIRE_THROWS(p1->SetVoiceSignature(""));

        REQUIRE_NOTHROW(p1->SetPreferredLanguage("en-us"));
        REQUIRE_THROWS(p1->SetPreferredLanguage(""));

        recognizer->AddParticipant(p1);

        recognizer->StartTranscribingAsync().get();
        WaitForResult(result->ready.get_future(), 15min);
        recognizer->StopTranscribingAsync().get();

        INFO(GetText(result->phrases));
        SPXTEST_REQUIRE(!result->phrases.empty());
        SPXTEST_REQUIRE(VerifyText(result->phrases[0].Text, katieSteve.m_utterance));
    }
    SPXTEST_SECTION("AddParticipants")
    {
        auto result = make_shared<RecoPhrases>();
        ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

        recognizer->SetConversationId("Conversation12345");

        auto p1 = Participant::From("katie@example.com", "en-us", katieVoiceSignature);
        recognizer->AddParticipant(p1);
        auto p2 = Participant::From("steve@example.com", "en-us", steveVoiceSignature);
        recognizer->AddParticipant(p2);

        recognizer->StartTranscribingAsync().get();
        WaitForResult(result->ready.get_future(), 15min);
        recognizer->StopTranscribingAsync().get();

        INFO(GetText(result->phrases));
        SPXTEST_REQUIRE(!result->phrases.empty());
        SPXTEST_REQUIRE(VerifyText(result->phrases[0].Text, katieSteve.m_utterance));
    }
    SPXTEST_SECTION("AddRemoveParticipants")
    {
        auto result = make_shared<RecoPhrases>();
        ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

        recognizer->SetConversationId("AddRemoveParticipants");

        auto p1 = Participant::From("id1", "en-us", katieVoiceSignature);
        recognizer->AddParticipant(p1);
        auto p2 = Participant::From("id2", "en-us", steveVoiceSignature);
        recognizer->AddParticipant(p2);

        REQUIRE_NOTHROW(recognizer->RemoveParticipant(p1));
        REQUIRE_NOTHROW(recognizer->AddParticipant(p2));
        REQUIRE_NOTHROW(recognizer->AddParticipant(p1));
        REQUIRE_NOTHROW(recognizer->RemoveParticipant(p1));

        recognizer->StartTranscribingAsync().get();
        WaitForResult(result->ready.get_future(), 15min);
        recognizer->StopTranscribingAsync().get();

        INFO(GetText(result->phrases));
        SPXTEST_REQUIRE(!result->phrases.empty());
        SPXTEST_REQUIRE(VerifyText(result->phrases[0].Text, katieSteve.m_utterance));
    }
    // a meeting without participant should still be fine!
    SPXTEST_SECTION("conversation_with_no_participant")
    {
        auto result = make_shared<RecoPhrases>();
        ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

        recognizer->SetConversationId("ConversationWithNoparticipants");
        recognizer->StartTranscribingAsync();

        WaitForResult(result->ready.get_future(), 15min);
        recognizer->StopTranscribingAsync().get();

        INFO(GetText(result->phrases));
        SPXTEST_REQUIRE(!result->phrases.empty());
        SPXTEST_REQUIRE(VerifyText(result->phrases[0].Text, katieSteve.m_utterance));
    }
}

TEST_CASE("conversation_inroom_8_channel_audio_push", "[.][int][prod]")
{
    auto audioEndpoint = Config::InroomEndpoint;
    audioEndpoint += "/multiaudio";
    auto config = SpeechConfig::FromEndpoint(audioEndpoint, Keys::ConversationTranscriber);

    std::shared_ptr<PushAudioInputStream> pushAudio;
    auto audioInput = CreateAudioPushUsingKatieSteveFile(pushAudio);

    string katieVoiceSignature, steveVoiceSignature;
    CreateVoiceSignatures(&katieVoiceSignature, &steveVoiceSignature);
    auto katie = Participant::From("katie@example.com", "en-us", katieVoiceSignature);

    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);
    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);
    recognizer->SetConversationId("Meeting12345");

    // add the speaker1 to usp
    recognizer->AddParticipant(katie);

    recognizer->StartTranscribingAsync().get();

    auto reader = std::make_shared<CSpxWavFileReader>();

    reader->Open(PAL::ToWString(katieSteve.m_inputDataFilename).c_str());
    SPXWAVEFORMATEX format;
    reader->GetFormat(&format, sizeof(SPXWAVEFORMATEX));
    auto bytesPerSample = format.wBitsPerSample / 8;
    auto samplesPerSec = format.nSamplesPerSec;
    auto framesPerSec = 10;
    auto channels = format.nChannels;
    SPX_DBG_ASSERT(channels >= 1);

    auto bytesPerFrame = samplesPerSec / framesPerSec * bytesPerSample * channels;
    auto data = SpxAllocSharedAudioBuffer(bytesPerFrame);

    auto  sleepBetween = 5ms;
    auto cbAudio = 0;
    do
    {
        // SetProperty before calling Write
        pushAudio->SetProperty(PropertyId::DataBuffer_UserId, "SpeakerA");

        cbAudio = reader->Read(data.get(), bytesPerFrame);
        pushAudio->Write(data.get(), cbAudio);
        std::this_thread::sleep_for(sleepBetween);
    } while (cbAudio != 0);

    pushAudio->Close();

    WaitForResult(result->ready.get_future(), 15min);
    recognizer->StopTranscribingAsync().get();

    INFO(GetText(result->phrases));
    SPXTEST_REQUIRE(!result->phrases.empty());
    SPXTEST_REQUIRE(VerifyText(result->phrases[0].Text, katieSteve.m_utterance));
}
TEST_CASE("conversation_online_pull_stream", "[.][int][prod]")
{
    REQUIRE(!Config::OnlineEndpoint.empty());
    REQUIRE(!Keys::ConversationTranscriber.empty());
    auto config = SpeechConfig::FromEndpoint(Config::OnlineEndpoint, Keys::ConversationTranscriber);

    weather.UpdateFullFilename(Config::InputDir);
    auto fs = OpenWaveFile(weather.m_inputDataFilename);

    // Create the "pull stream" object with C++ lambda callbacks
    auto pullStream = AudioInputStream::CreatePullStream(
        AudioStreamFormat::GetWaveFormatPCM(16000, 16, 1),
        [&fs](uint8_t* buffer, uint32_t size) -> int { return (int)ReadBuffer(fs, buffer, size); },
        [=]() {},
        [=](PropertyId propertyId) -> SPXSTRING {
        if (propertyId == PropertyId::DataBuffer_TimeStamp)
        {
            return CreateTimestamp();
        }
        else if (propertyId == PropertyId::DataBuffer_UserId)
        {
            return "SP042@speechdemo.onmicrosoft.com";
        }
        else
        {
            return "";
        }
    }
    );

    auto audioInput = AudioConfig::FromStreamInput(pullStream);
    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);

    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

    SPXTEST_SECTION("end_conversation")
    {
        REQUIRE_NOTHROW(recognizer->SetConversationId("Conversation12345"));

        REQUIRE_NOTHROW(recognizer->EndConversationAsync().get());
    }
    SPXTEST_SECTION("conversation_id")
    {
        REQUIRE_THROWS(recognizer->SetConversationId(""));
        std::string orig_id {"123"};
        REQUIRE_NOTHROW(recognizer->SetConversationId(orig_id));
        auto got_id = recognizer->GetConversationId();
        SPXTEST_REQUIRE(orig_id == got_id);
    }
    SPXTEST_SECTION("all_features")
    {
        recognizer->SetConversationId("Conversation12345");

        auto participant = Participant::From("SP042@speechdemo.onmicrosoft.com", "en-us");
        recognizer->AddParticipant(participant);

        // set iCalUid
        recognizer->Properties.SetProperty("iCalUid", "asdf");

        // set callId
        recognizer->Properties.SetProperty("callId", "asdf");

        // set organizer
        recognizer->Properties.SetProperty("organizer", "SP049@somedomain.com");

        // turn on audio recording
        recognizer->Properties.SetProperty("audiorecording", "on");

        recognizer->StartTranscribingAsync().get();

        WaitForResult(result->ready.get_future(), 15min);
        recognizer->StopTranscribingAsync().get();
        SPXTEST_REQUIRE(result->phrases.size() >= 1);
        SPXTEST_REQUIRE(result->phrases[0].Text == weather.m_utterance);
    }
}
TEST_CASE("conversation_online_1_channel_file", "[.][int][prod]")
{
    auto config = SpeechConfig::FromEndpoint(Config::OnlineEndpoint, Keys::ConversationTranscriber);

    weather.UpdateFullFilename(Config::InputDir);
    auto audioInput = AudioConfig::FromWavFileInput(weather.m_inputDataFilename);
    auto recognizer = ConversationTranscriber::FromConfig(config, audioInput);
    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);
    auto p = Participant::From("conversation_online_1_channel_file", "en-us");
    StartMeetingAndVerifyResult(recognizer.get(), p, move(result), weather.m_utterance);
}
#if 0
TEST_CASE("conversation_online_microphone", "[api][cxx]")
{
    auto config = SpeechConfig::FromEndpoint(Config::OnlineEndpoint, Keys::Speech);
    auto recognizer = ConversationTranscriber::FromConfig(config);

    auto result = make_shared<RecoPhrases>();
    ConnectCallbacks<ConversationTranscriber, ConversationTranscriptionEventArgs, ConversationTranscriptionCanceledEventArgs>(recognizer.get(), result);

    recognizer->SetConversationId("ConversationWithNoparticipants");
    auto  participant = recognizer->AddParticipant("john@microsoft.com");

    recognizer->StartTranscribingAsync().get();

    WaitForResult(result->ready.get_future(), 30s);
    recognizer->StopTranscribingAsync().get();
    SPXTEST_REQUIRE(result->phrases.size() >= 1);
    SPXTEST_REQUIRE(result->phrases[0].Text == weather.m_utterance);
}
#endif
