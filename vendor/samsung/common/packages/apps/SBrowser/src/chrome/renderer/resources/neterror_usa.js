function toggleHelpBox1() {
  var helpBoxOuter1 = document.getElementById('help-box-outer1');
  var helpBoxOuter2 = document.getElementById('help-box-outer2');
  var helpBoxOuter3 = document.getElementById('help-box-outer3');
  var helpBoxOuter4 = document.getElementById('help-box-outer4');
  helpBoxOuter1.classList.toggle('hidden');
  if (helpBoxOuter2.classList.contains('hidden')==false)
	helpBoxOuter2.classList.toggle('hidden');
  if (helpBoxOuter3.classList.contains('hidden')==false)
	helpBoxOuter3.classList.toggle('hidden');
  if (helpBoxOuter4.classList.contains('hidden')==false)
	helpBoxOuter4.classList.toggle('hidden');
}

function toggleHelpBox2() {
  var helpBoxOuter1 = document.getElementById('help-box-outer1');
  var helpBoxOuter2 = document.getElementById('help-box-outer2');
  var helpBoxOuter3 = document.getElementById('help-box-outer3');
  var helpBoxOuter4 = document.getElementById('help-box-outer4');
  helpBoxOuter2.classList.toggle('hidden');
  if (helpBoxOuter1.classList.contains('hidden')==false)
	helpBoxOuter1.classList.toggle('hidden');
  if (helpBoxOuter3.classList.contains('hidden')==false)
	helpBoxOuter3.classList.toggle('hidden');
  if (helpBoxOuter4.classList.contains('hidden')==false)
	helpBoxOuter4.classList.toggle('hidden');
}

function toggleHelpBox3() {
  var helpBoxOuter1 = document.getElementById('help-box-outer1');
  var helpBoxOuter2 = document.getElementById('help-box-outer2');
  var helpBoxOuter3 = document.getElementById('help-box-outer3');
  var helpBoxOuter4 = document.getElementById('help-box-outer4');
  helpBoxOuter3.classList.toggle('hidden');
  if (helpBoxOuter1.classList.contains('hidden')==false)
	helpBoxOuter1.classList.toggle('hidden');
  if (helpBoxOuter2.classList.contains('hidden')==false)
	helpBoxOuter2.classList.toggle('hidden');
  if (helpBoxOuter4.classList.contains('hidden')==false)
	helpBoxOuter4.classList.toggle('hidden');
}

function toggleHelpBox4() {
  var helpBoxOuter1 = document.getElementById('help-box-outer1');
  var helpBoxOuter2 = document.getElementById('help-box-outer2');
  var helpBoxOuter3 = document.getElementById('help-box-outer3');
  var helpBoxOuter4 = document.getElementById('help-box-outer4');
  helpBoxOuter4.classList.toggle('hidden');
  if (helpBoxOuter1.classList.contains('hidden')==false)
	helpBoxOuter1.classList.toggle('hidden');
  if (helpBoxOuter2.classList.contains('hidden')==false)
	helpBoxOuter2.classList.toggle('hidden');
  if (helpBoxOuter3.classList.contains('hidden')==false)
	helpBoxOuter3.classList.toggle('hidden');
}

// Subframes use a different layout but the same html file.  This is to make it
// easier to support platforms that load the error page via different
// mechanisms (Currently just iOS).
if (window.top.location != window.location)
  document.documentElement.setAttribute('subframe', '');