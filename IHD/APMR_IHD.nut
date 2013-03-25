//The APMR In-Home Display

local onpeak = 0;
local watts = 0;
local watt_hours = 0;
local tier1 = 0;
local tier2 = 0;
local ledState = 0;
local onColour = 0;
local pauseLen = 2.0;

local tierRequestOutput = OutputPort("Colour Tiers", "number");

hardware.pin7.configure(DIGITAL_OUT_OD_PULLUP); // red
hardware.pin8.configure(DIGITAL_OUT_OD_PULLUP); // green
hardware.pin9.configure(DIGITAL_OUT_OD_PULLUP); // yellow

function updateNode()
{
    server.show(watts + "W, Peak? " + onpeak + ", Tiers [" + tier1 + "," + tier2 + "]");
}

function updateAmbients()
{
    if(onpeak)
        ledState = ledState ? 0 : 1;
    else
        ledState = 0;
 
    if(watts <= tier1)
    {
        hardware.pin7.write(1);
        hardware.pin8.write(ledState);
        hardware.pin9.write(1);        
    }
    else if(watts <= tier2)
    {
        hardware.pin7.write(1);
        hardware.pin8.write(1);
        hardware.pin9.write(ledState);        
    }
    else
    {
        hardware.pin7.write(ledState);
        hardware.pin8.write(1);
        hardware.pin9.write(1);        
    }
    
    if(ledState)
        imp.wakeup(0.25, updateAmbients);
    else
        imp.wakeup(1.75, updateAmbients);
}

function updateTiers()
{
    tierRequestOutput.set(1);    
    imp.wakeup(3600, updateTiers);
}

function blinkInit()
{
    hardware.pin7.write(1);
    hardware.pin8.write(1);
    hardware.pin9.write(1);
    
    if(tier1 > 0 && tier2 > 0)
    {
        updateAmbients();
        return;
    }
    
    pauseLen -= 0.250;
    if(pauseLen < 0.250) pauseLen = 0.250;
    
    switch(++onColour % 3)
    {
        case 0:
            hardware.pin7.write(0);
            break;
        case 1:
            hardware.pin8.write(0);
            break;
        case 2:
            hardware.pin9.write(0);
            break;
        
    }
 
    imp.wakeup(pauseLen, blinkInit);
}

function init()
{
    blinkInit();
    updateTiers();
    updateAmbients();
    updateNode();
}

class PowerInput extends InputPort
{
    name = "Real Power Reading";
    type = "array";
 
    constructor()
    {
        base.constructor();
    }
 
    function set(v)
    {
        watts = v[0];
        watt_hours = v[1];
        updateNode();
    }
}

class PeakInput extends InputPort
{
    name = "Pricing Signal";
    type = "number";
 
    constructor()
    {
        base.constructor();
    }
 
    function set(v)
    {
        onpeak = v;
        updateNode();
    }
}

class ColourTiersInput extends InputPort
{
    name = "Colour Tiers";
    type = "array";
 
    constructor()
    {
        base.constructor();
    }
 
    function set(v)
    {
        tier1 = v[0];
        tier2 = v[1];
        updateNode();
    }
}

imp.configure("APMR IHD", [PowerInput(), PeakInput(), ColourTiersInput()], [tierRequestOutput]);

init();
// End of code.
