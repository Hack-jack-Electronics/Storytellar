//------------------------------------------------------------------------------------------------------------------------
// 
// Title: SD Card Multi-File Wav Player with Speed Control
//
// Description:
//    Modified version with multiple file support and serial control
//
//------------------------------------------------------------------------------------------------------------------------

#include "SD.h"                         // SD Card library, usually part of the standard install
#include "driver/i2s.h"                 // Library of I2S routines, comes with ESP32 standard install

//------------------------------------------------------------------------------------------------------------------------
// Defines
 
//    SD Card
          #define SD_CS          5          // SD Card chip select
   
//    I2S
          #define I2S_DOUT      27          // i2S Data out oin
          #define I2S_BCLK      26          // Bit clock
          #define I2S_LRC       25          // Left/Right clock, also known as Frame clock or word select
          #define I2S_NUM       0           // i2s port number

// Wav File reading
          #define NUM_BYTES_TO_READ_FROM_FILE 32    // How many bytes to read from wav file at a time

// Speed Control Options - Choose ONE method
          #define SPEED_METHOD 1            // 1 = Sample rate division, 2 = Sample skipping, 3 = Delay insertion

// Speed Control Settings
          #define SPEED_DIVISOR 2           // For method 1: Divide sample rate by this (2 = half speed, 4 = quarter speed)
          #define SKIP_SAMPLES 1            // For method 2: Skip every N samples (1 = skip every other sample)
          #define PLAYBACK_DELAY 20         // For method 3: Delay in milliseconds between buffer sends

// File Management
          #define MAX_FILES 20              // Maximum number of WAV files to handle
          #define MAX_FILENAME_LENGTH 50    // Maximum length for filename

//------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------
// structures and also variables
//  I2S configuration

      static const i2s_config_t i2s_config = 
      {
          .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
          .sample_rate = 44100,                                 // Note, this will be changed later
          .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
          .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
          .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
          .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,             // high interrupt priority
          .dma_buf_count = 8,                                   // 8 buffers
          .dma_buf_len = 64,                                    // 64 bytes per buffer, so 8K of buffer space
          .use_apll=0,
          .tx_desc_auto_clear= true, 
          .fixed_mclk=-1    
      };
      
      static const i2s_pin_config_t pin_config = 
      {
          .bck_io_num = I2S_BCLK,                           // The bit clock connectiom, goes to pin 27 of ESP32
          .ws_io_num = I2S_LRC,                             // Word select, also known as word select or left right clock
          .data_out_num = I2S_DOUT,                         // Data out from the ESP32, connect to DIN on 38357A
          .data_in_num = I2S_PIN_NO_CHANGE                  // we are not interested in I2S data into the ESP32
      };
      
      struct WavHeader_Struct
      {
          //   RIFF Section    
          char RIFFSectionID[4];      // Letters "RIFF"
          uint32_t Size;              // Size of entire file less 8
          char RiffFormat[4];         // Letters "WAVE"
          
          //   Format Section    
          char FormatSectionID[4];    // letters "fmt"
          uint32_t FormatSize;        // Size of format section less 8
          uint16_t FormatID;          // 1=uncompressed PCM
          uint16_t NumChannels;       // 1=mono,2=stereo
          uint32_t SampleRate;        // 44100, 16000, 8000 etc.
          uint32_t ByteRate;          // =SampleRate * Channels * (BitsPerSample/8)
          uint16_t BlockAlign;        // =Channels * (BitsPerSample/8)
          uint16_t BitsPerSample;     // 8,16,24 or 32
        
          // Data Section
          char DataSectionID[4];      // The letters "data"
          uint32_t DataSize;          // Size of the data that follows
      }WavHeader;

//  Global Variables/objects    
    
    File WavFile;                                 // Object for root of SD card directory
    static const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number    

    // File management variables
    String wavFiles[MAX_FILES];                   // Array to store WAV file names
    int fileCount = 0;                           // Number of WAV files found
    int currentFileIndex = -1;                   // Currently playing file index (-1 = no file)
    bool isPlaying = false;                      // Is a file currently playing?
    bool autoPlay = false;                       // Auto-play mode (play all files sequentially)
    String inputString = "";                     // String to hold incoming serial data
    bool stringComplete = false;                 // Whether the string is complete

//------------------------------------------------------------------------------------------------------------------------

void setup() {    
    Serial.begin(115200);                               // Used for info/debug
    Serial.println("Multi-File WAV Player Starting...");
    
    SDCardInit();
    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    i2s_set_pin(i2s_num, &pin_config);
    
    // Scan SD card for WAV files
    scanWavFiles();
    
    // Print available commands
    printHelp();
}

void loop()
{    
    // Check for serial input
    handleSerialInput();
    
    // Handle playback
    if(isPlaying && WavFile)
    {
        PlayWav();                                            
        
        // Check if file finished playing - use file position instead of calculated end
        if(!WavFile.available() || WavFile.position() >= WavFile.size())
        {
            Serial.println("File finished playing.");
            stopPlayback();
            
            // Auto-play next file if enabled
            if(autoPlay)
            {
                playNextFile();
            }
        }
    }
}

void scanWavFiles()
{
    Serial.println("Scanning SD card for WAV files...");
    fileCount = 0;
    
    File root = SD.open("/");
    if(!root)
    {
        Serial.println("Failed to open root directory");
        return;
    }
    
    File file = root.openNextFile();
    while(file && fileCount < MAX_FILES)
    {
        if(!file.isDirectory())
        {
            String fileName = file.name();
            if(fileName.endsWith(".wav") || fileName.endsWith(".WAV"))
            {
                wavFiles[fileCount] = "/" + fileName;
                Serial.print(fileCount + 1);
                Serial.print(": ");
                Serial.println(wavFiles[fileCount]);
                fileCount++;
            }
        }
        file = root.openNextFile();
    }
    
    Serial.print("Found ");
    Serial.print(fileCount);
    Serial.println(" WAV files.");
}

void printHelp()
{
    Serial.println("\n=== WAV Player Commands ===");
    Serial.println("1-" + String(fileCount) + ": Play specific file by number");
    Serial.println("list: Show all available files");
    Serial.println("stop: Stop current playback");
    Serial.println("auto: Toggle auto-play mode");
    Serial.println("next: Play next file");
    Serial.println("prev: Play previous file");
    Serial.println("help: Show this help");
    Serial.println("========================\n");
}

void handleSerialInput()
{
    while(Serial.available())
    {
        char inChar = (char)Serial.read();
        if(inChar == '\n' || inChar == '\r')
        {
            stringComplete = true;
        }
        else
        {
            inputString += inChar;
        }
    }
    
    if(stringComplete)
    {
        inputString.trim();
        processCommand(inputString);
        inputString = "";
        stringComplete = false;
    }
}

void processCommand(String command)
{
    command.toLowerCase();
    
    if(command == "list")
    {
        listFiles();
    }
    else if(command == "stop")
    {
        stopPlayback();
    }
    else if(command == "help")
    {
        printHelp();
    }
    else if(command == "auto")
    {
        autoPlay = !autoPlay;
        Serial.print("Auto-play mode: ");
        Serial.println(autoPlay ? "ON" : "OFF");
    }
    else if(command == "next")
    {
        playNextFile();
    }
    else if(command == "prev")
    {
        playPreviousFile();
    }
    else
    {
        // Check if it's a number
        int fileNum = command.toInt();
        if(fileNum >= 1 && fileNum <= fileCount)
        {
            playFile(fileNum - 1);
        }
        else
        {
            Serial.println("Invalid command. Type 'help' for available commands.");
        }
    }
}

void listFiles()
{
    Serial.println("\nAvailable WAV files:");
    for(int i = 0; i < fileCount; i++)
    {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(wavFiles[i]);
        if(i == currentFileIndex)
        {
            Serial.print(" (Currently playing)");
        }
        Serial.println();
    }
    Serial.println();
}

void playFile(int index)
{
    if(index < 0 || index >= fileCount)
    {
        Serial.println("Invalid file index");
        return;
    }
    
    // Stop current playback
    stopPlayback();
    
    // Open new file
    WavFile = SD.open(wavFiles[index].c_str());
    if(!WavFile)
    {
        Serial.print("Could not open file: ");
        Serial.println(wavFiles[index]);
        return;
    }
    
    // Get file size for data size calculation
    uint32_t fileSize = WavFile.size();
    
    // Read WAV header
    WavFile.read((byte *) &WavHeader, 44);
    
    // Fix corrupted data size if needed
    if(WavHeader.DataSize == 0xFFFFFFFF || WavHeader.DataSize == 0 || WavHeader.DataSize > (fileSize - 44))
    {
        WavHeader.DataSize = fileSize - 44;  // Calculate actual data size
        Serial.print("Fixed corrupted data size to: ");
        Serial.println(WavHeader.DataSize);
    }
    
    if(!ValidWavData(&WavHeader))
    {
        Serial.println("Invalid WAV file format");
        WavFile.close();
        return;
    }
    
    // Set up I2S with correct sample rate
    uint32_t adjustedSampleRate = WavHeader.SampleRate;
    
    #if SPEED_METHOD == 1
        adjustedSampleRate = WavHeader.SampleRate / SPEED_DIVISOR;
        Serial.print("Original sample rate: "); Serial.println(WavHeader.SampleRate);
        Serial.print("Adjusted sample rate: "); Serial.println(adjustedSampleRate);
    #endif
    
    i2s_set_sample_rates(i2s_num, adjustedSampleRate);
    
    currentFileIndex = index;
    isPlaying = true;
    
    Serial.print("Now playing: ");
    Serial.println(wavFiles[index]);
    DumpWAVHeader(&WavHeader);
}

void stopPlayback()
{
    if(WavFile)
    {
        WavFile.close();
    }
    isPlaying = false;
    currentFileIndex = -1;
    Serial.println("Playback stopped.");
}

void playNextFile()
{
    if(fileCount == 0)
    {
        Serial.println("No files available");
        return;
    }
    
    int nextIndex = (currentFileIndex + 1) % fileCount;
    if(currentFileIndex == -1) nextIndex = 0;
    
    playFile(nextIndex);
}

void playPreviousFile()
{
    if(fileCount == 0)
    {
        Serial.println("No files available");
        return;
    }
    
    int prevIndex = (currentFileIndex - 1 + fileCount) % fileCount;
    if(currentFileIndex == -1) prevIndex = fileCount - 1;
    
    playFile(prevIndex);
}

void PlayWav()
{
    static bool ReadingFile=true;                       // True if reading file from SD. false if filling I2S buffer
    static byte Samples[NUM_BYTES_TO_READ_FROM_FILE];   // Memory allocated to store the data read in from the wav file
    static uint16_t BytesRead;                          // Num bytes actually read from the wav file

    if(ReadingFile)                                     // Read next chunk of data in from file if needed
    {
        BytesRead=ReadFile(Samples);                    // Read data into our memory buffer, return num bytes read in
        ReadingFile=false;                              // Switch to sending the buffer to the I2S
    }
    else
    {
        ReadingFile=FillI2SBuffer(Samples,BytesRead);        // We keep calling this routine until it returns true
        
        #if SPEED_METHOD == 3
            // Add delay between buffer sends to slow down playback
            if(ReadingFile)  // Only delay when we've finished sending current buffer
                delay(PLAYBACK_DELAY);
        #endif
    }
}

uint16_t ReadFile(byte* Samples)
{
    static uint32_t BytesReadSoFar=0;                   // Number of bytes read from file so far
    static uint16_t SampleSkipCounter=0;                // Counter for sample skipping method
    uint16_t BytesToRead;                               // Number of bytes to read from the file
    
    // Check if file has data available
    if(!WavFile.available())
    {
        return 0;  // No more data to read
    }
    
    // Calculate how much to read
    uint32_t remainingInFile = WavFile.size() - WavFile.position();
    if(remainingInFile < NUM_BYTES_TO_READ_FROM_FILE)
    {
        BytesToRead = remainingInFile;
    }
    else
    {
        BytesToRead = NUM_BYTES_TO_READ_FROM_FILE;
    }
    
    if(BytesToRead == 0)
        return 0;
        
    WavFile.read(Samples,BytesToRead);                  // Read in the bytes from the file
    
    #if SPEED_METHOD == 2
        // Sample skipping method - skip samples to slow down playback
        if(SKIP_SAMPLES > 0)
        {
            // Simple sample skipping - duplicate every other sample
            for(int i = BytesToRead-2; i >= 0; i-=4) // Work backwards, 4 bytes per stereo 16-bit sample
            {
                if(SampleSkipCounter >= SKIP_SAMPLES)
                {
                    // Duplicate this sample by copying it to the next position
                    if(i+4 < BytesToRead)
                    {
                        Samples[i+4] = Samples[i];
                        Samples[i+5] = Samples[i+1];
                        Samples[i+6] = Samples[i+2];
                        Samples[i+7] = Samples[i+3];
                    }
                    SampleSkipCounter = 0;
                }
                SampleSkipCounter++;
            }
        }
    #endif
    
    return BytesToRead;                                 // return the number of bytes read into buffer
}

bool FillI2SBuffer(byte* Samples,uint16_t BytesInBuffer)
{
    size_t BytesWritten;                        // Returned by the I2S write routine, 
    static uint16_t BufferIdx=0;                // Current pos of buffer to output next
    uint8_t* DataPtr;                           // Point to next data to send to I2S
    uint16_t BytesToSend;                       // Number of bytes to send to I2S
    
    DataPtr=Samples+BufferIdx;                               // Set address to next byte in buffer to send out
    BytesToSend=BytesInBuffer-BufferIdx;                     // This is amount to send (total less what we've already sent)
    i2s_write(i2s_num,DataPtr,BytesToSend,&BytesWritten,1);  // Send the bytes, wait 1 RTOS tick to complete
    BufferIdx+=BytesWritten;                                 // increasue by number of bytes actually written
    
    if(BufferIdx>=BytesInBuffer)                 
    {
        // sent out all bytes in buffer, reset and return true to indicate this
        BufferIdx=0; 
        return true;                             
    }
    else
        return false;       // Still more data to send to I2S so return false to indicate this
}

void SDCardInit()
{        
    pinMode(SD_CS, OUTPUT); 
    digitalWrite(SD_CS, HIGH); // SD card chips select, must use GPIO 5 (ESP32 SS)
    if(!SD.begin(SD_CS))
    {
        Serial.println("Error talking to SD card!");
        while(true);                  // end program
    }
}

bool ValidWavData(WavHeader_Struct* Wav)
{
    if(memcmp(Wav->RIFFSectionID,"RIFF",4)!=0) 
    {    
        Serial.println("Invalid data - Not RIFF format");
        return false;        
    }
    if(memcmp(Wav->RiffFormat,"WAVE",4)!=0)
    {
        Serial.println("Invalid data - Not Wave file");
        return false;           
    }
    if(memcmp(Wav->FormatSectionID,"fmt",3)!=0) 
    {
        Serial.println("Invalid data - No format section found");
        return false;       
    }
    if(memcmp(Wav->DataSectionID,"data",4)!=0) 
    {
        Serial.println("Invalid data - data section not found");
        return false;      
    }
    if(Wav->FormatID!=1) 
    {
        Serial.println("Invalid data - format Id must be 1");
        return false;                          
    }
    if(Wav->FormatSize!=16) 
    {
        Serial.println("Invalid data - format section size must be 16.");
        return false;                          
    }
    if((Wav->NumChannels!=1)&(Wav->NumChannels!=2))
    {
        Serial.println("Invalid data - only mono or stereo permitted.");
        return false;   
    }
    if(Wav->SampleRate>48000) 
    {
        Serial.println("Invalid data - Sample rate cannot be greater than 48000");
        return false;                       
    }
    if((Wav->BitsPerSample!=8)& (Wav->BitsPerSample!=16)) 
    {
        Serial.println("Invalid data - Only 8 or 16 bits per sample permitted.");
        return false;                        
    }
    return true;
}

void DumpWAVHeader(WavHeader_Struct* Wav)
{
    if(memcmp(Wav->RIFFSectionID,"RIFF",4)!=0)
    {
        Serial.print("Not a RIFF format file - ");    
        PrintData(Wav->RIFFSectionID,4);
        return;
    } 
    if(memcmp(Wav->RiffFormat,"WAVE",4)!=0)
    {
        Serial.print("Not a WAVE file - ");  
        PrintData(Wav->RiffFormat,4);  
        return;
    }  
    if(memcmp(Wav->FormatSectionID,"fmt",3)!=0)
    {
        Serial.print("fmt ID not present - ");
        PrintData(Wav->FormatSectionID,3);      
        return;
    } 
    if(memcmp(Wav->DataSectionID,"data",4)!=0)
    {
        Serial.print("data ID not present - "); 
        PrintData(Wav->DataSectionID,4);
        return;
    }  
    // All looks good, dump the data
    Serial.print("Total size :");Serial.println(Wav->Size);
    Serial.print("Format section size :");Serial.println(Wav->FormatSize);
    Serial.print("Wave format :");Serial.println(Wav->FormatID);
    Serial.print("Channels :");Serial.println(Wav->NumChannels);
    Serial.print("Sample Rate :");Serial.println(Wav->SampleRate);
    Serial.print("Byte Rate :");Serial.println(Wav->ByteRate);
    Serial.print("Block Align :");Serial.println(Wav->BlockAlign);
    Serial.print("Bits Per Sample :");Serial.println(Wav->BitsPerSample);
    Serial.print("Data Size :");Serial.println(Wav->DataSize);
}

void PrintData(const char* Data,uint8_t NumBytes)
{
    for(uint8_t i=0;i<NumBytes;i++)
        Serial.print(Data[i]); 
        Serial.println();  
}