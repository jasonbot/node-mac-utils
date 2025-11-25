#!/usr/bin/env node

const macUtils = require('./index.js');

console.log('Testing Microphone Authorization Status\n');
console.log('=========================================\n');

try {
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
      console.log('   First microphone access attempt will trigger permission prompt.');
      break;
    case 'Restricted':
      console.log('⚠️  Microphone access is restricted by system policies.');
      break;
    case 'Denied':
      console.log('❌ Microphone access was denied.');
      console.log('   User needs to grant permission in System Preferences > Privacy & Security > Microphone');
      break;
    case 'Authorized':
      console.log('✅ Microphone access is authorized!');
      break;
    default:
      console.log('⚠️  Unknown status received');
  }

  console.log('\n=========================================');
  console.log(`Platform: ${process.platform}`);

  if (process.platform !== 'darwin') {
    console.log('\nNote: On non-macOS platforms, this always returns "Authorized" as a no-op.');
  }

} catch (error) {
  console.error('❌ Error testing microphone authorization:', error);
  process.exit(1);
}
