#!/usr/bin/env node

const macUtils = require('./index.js');

async function testRequest() {
  console.log('Testing requestMicrophoneAccess() Promise API\n');
  console.log('=============================================\n');

  try {
    console.log('Before request - Status:', macUtils.getMicrophoneAuthorizationStatus());

    console.log('\nCalling requestMicrophoneAccess()...');
    const granted = await macUtils.requestMicrophoneAccess();

    console.log('\n✅ Promise resolved successfully!');
    console.log('Granted:', granted);
    console.log('Type:', typeof granted);

    console.log('\nAfter request - Status:', macUtils.getMicrophoneAuthorizationStatus());

    console.log('\n=============================================');
    console.log('✅ Test passed! The Promise API works correctly.');

  } catch (error) {
    console.error('❌ Error:', error);
    process.exit(1);
  }
}

testRequest();
