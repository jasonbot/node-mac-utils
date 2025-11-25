#!/usr/bin/env node

const macUtils = require('./index.js');

async function testMicrophoneAuth() {
  console.log('Testing Microphone Authorization\n');
  console.log('=========================================\n');

  try {
    // Test 1: Check current status
    console.log('1️⃣  Checking current authorization status...\n');

    const status = macUtils.getMicrophoneAuthorizationStatus();

    console.log(`Current Status: ${status}\n`);

    console.log('Status Meanings:');
    console.log('  NotDetermined - User hasn\'t been asked for permission yet');
    console.log('  Restricted    - App isn\'t permitted to use microphone (parental controls, etc.)');
    console.log('  Denied        - User explicitly denied permission');
    console.log('  Authorized    - User explicitly granted permission\n');

    // Provide contextual feedback
    switch (status) {
      case 'NotDetermined':
        console.log('ℹ️  The app hasn\'t requested microphone access yet.');
        console.log('   Use requestMicrophoneAccess() to trigger the permission prompt.\n');
        break;
      case 'Restricted':
        console.log('⚠️  Microphone access is restricted by system policies.\n');
        break;
      case 'Denied':
        console.log('❌ Microphone access was denied.');
        console.log('   User needs to grant permission in System Preferences > Privacy & Security > Microphone\n');
        break;
      case 'Authorized':
        console.log('✅ Microphone access is authorized!\n');
        break;
      default:
        console.log('⚠️  Unknown status received\n');
    }

    // Test 2: Request access (if needed)
    if (status === 'NotDetermined') {
      console.log('2️⃣  Requesting microphone access...');
      console.log('   (This will show a system permission dialog on macOS)\n');

      const granted = await macUtils.requestMicrophoneAccess();

      if (granted) {
        console.log('✅ Microphone access was GRANTED!\n');
      } else {
        console.log('❌ Microphone access was DENIED.\n');
      }

      // Check status again
      const newStatus = macUtils.getMicrophoneAuthorizationStatus();
      console.log(`New Status: ${newStatus}\n`);
    } else {
      console.log('2️⃣  Skipping access request (already determined)\n');
      console.log('   To test requestMicrophoneAccess():');
      console.log('   - Run: await requestMicrophoneAccess()');
      console.log('   - Returns a Promise<boolean> that resolves with true if granted\n');
    }

    console.log('=========================================');
    console.log(`Platform: ${process.platform}`);

    if (process.platform !== 'darwin') {
      console.log('\nNote: On non-macOS platforms, these methods are no-ops that return positive results.');
    }

  } catch (error) {
    console.error('❌ Error testing microphone authorization:', error);
    process.exit(1);
  }
}

testMicrophoneAuth();
