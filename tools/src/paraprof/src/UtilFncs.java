 /* 
   UtilFncs.java

   Title:      ParaProf
   Author:     Robert Bell
   Description:  Some useful functions for the system.
*/

package paraprof;
import java.util.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.event.*;
import java.lang.*;
import java.io.*;
import java.text.*;

public class UtilFncs{
  
    public static double adjustDoublePresision(double d, int precision){
	String result = null;
 	try{
	    String formatString = "##0.0";
	    for(int i=0;i<(precision-1);i++){
		formatString = formatString+"0";
	    }
	    if(d < 0.001){
		for(int i=0;i<4;i++){
		    formatString = formatString+"0";
		}
	    }
        
	    DecimalFormat dF = new DecimalFormat(formatString);
	    result = dF.format(d);
	}
	catch(Exception e){
		UtilFncs.systemError(e, null, "UF01");
	}
	return Double.parseDouble(result);
    }
    
    public static String getTestString(double inDouble, int precision){
      
	//This method comes up with a rough estimation.  The drawing windows do not
	//need to be absolutely accurate.
      
	String returnString = "";
	for(int i=0;i<precision;i++){
	    returnString = returnString + " ";
	}
      
	long tmpLong = Math.round(inDouble);
	returnString = Long.toString(tmpLong) + returnString;
      
	return returnString;
    }

    //This method is used in a number of windows to determine the actual output string
    //displayed. Current types are:
    //0 - microseconds
    //1 - milliseconds
    //2 - seconds
    //3 - hr:min:sec
    //At present, the passed in double value is assumed to be in microseconds.
    public static String getOutputString(int type, double d, int precision){
	switch(type){
	case 0:
	    return (Double.toString(UtilFncs.adjustDoublePresision(d, precision)));
	case 1:
	    return (Double.toString(UtilFncs.adjustDoublePresision((d/1000), precision)));
	case 2:
	    return (Double.toString(UtilFncs.adjustDoublePresision((d/1000000), precision)));
	case 3:
	    int hr = 0;
	    int min = 0;
	    hr = (int) (d/3600000000.00);
	    //Calculate the number of microseconds left after hours are subtracted.
	    d = d-hr*3600000000.00;
	    min = (int) (d/60000000.00);
	    //Calculate the number of microseconds left after minutess are subtracted.
	    d = d-min*60000000.00;
	    return (Integer.toString(hr)+":"+Integer.toString(min)+":"+Double.toString(UtilFncs.adjustDoublePresision((d/1000000), precision)));
	default:
	    UtilFncs.systemError(null, null, "Unexpected string type - UF02 value: " + type);
	}
	return null;
    }

    public static String getUnitsString(int type, boolean time, boolean derived){
	if(derived){
	   if(!time)
	       return "counts";
	   switch(type){
	   case 0:
	       return "Derived metric shown in microseconds format";
	   case 1:
	       return "Derived metric shown in milliseconds format";
	   case 2:
	       return "Derived metric shown in seconds format";
	   case 3:
	       return "Derived metric shown in hour:minute:seconds format"; 
	   }
	}
	else{
	    if(!time)
		return "counts";
	    switch(type){
	    case 0:
		return "microseconds";
	    case 1:
		return "milliseconds";
	    case 2:
		return "seconds";
	    case 3:
		return "hour:minute:seconds";
	    default:
		UtilFncs.systemError(null, null, "Unexpected string type - UF03 value: " + type);
	    }
	}
	return null;
    }

    public static String getValueTypeString(int type){
	switch(type){
	case 2:
	    return "exclusive";
	case 4:
	    return "inclusive";
	case 6:
	    return "number of calls";
	case 8:
	    return "number of subroutines";
	case 10:
	    return "per call value";
	case 12:
	    return "number of userevents";
	case 14:
	    return "min";
	case 16:
	    return "max";
	case 18:
	    return "mean";
	default:
	    UtilFncs.systemError(null, null, "Unexpected string type - UF04 value: " + type);
	}
	return null;
    }

    public static int exists(int[] ref, int i){
	if(ref == null)
	    return -1;
	int test = ref.length;
	for(int j=0;j<test;j++){
	    if(ref[j]==i)
		return j;
	}
	return -1;
    }

    public static int exists(Vector ref, int i){
	//Assuming a vector of Integers.
	if(ref == null)
	    return -1;
	Integer current = null;
	int test = ref.size();
	for(int j=0;j<test;j++){
	    current = (Integer) ref.elementAt(j);
	    if((current.intValue())==i)
		return j;
	}
	return -1;
    }

    //####################################
    //Error handling.
    //####################################
    public static boolean debug = false;

    public static void systemError(Object obj, Component component, String string){ 
	System.out.println("####################################");
	boolean quit = true; //Quit by default.
	if(obj != null){
	    if(obj instanceof Exception){
		Exception exception = (Exception) obj;
		if(UtilFncs.debug){
		    System.out.println(exception.toString());
		    exception.printStackTrace();
		    System.out.println("\n");
		}
		System.out.println("An error was detected: " + string);
		System.out.println(ParaProfError.contactString);
	    }
	    if(obj instanceof ParaProfError){
		ParaProfError paraProfError = (ParaProfError) obj;
		if(UtilFncs.debug){
		    if((paraProfError.showPopup)&&(paraProfError.popupString!=null))
			JOptionPane.showMessageDialog(paraProfError.component,
						      "ParaProf Error", paraProfError.popupString, JOptionPane.ERROR_MESSAGE);
		    if(paraProfError.exp!=null){
			System.out.println(paraProfError.exp.toString());
			paraProfError.exp.printStackTrace();
			System.out.println("\n");
		    }
		    if(paraProfError.location!=null)
			System.out.println("Location: " + paraProfError.location);
		    if(paraProfError.s0!=null)
			System.out.println(paraProfError.s0);
		    if(paraProfError.s1!=null)
			System.out.println(paraProfError.s1);
		    if(paraProfError.showContactString)
			System.out.println(ParaProfError.contactString);
		}
		else{
		    if((paraProfError.showPopup)&&(paraProfError.popupString!=null))
			JOptionPane.showMessageDialog(paraProfError.component,
						      "ParaProf Error", paraProfError.popupString, JOptionPane.ERROR_MESSAGE);
		    if(paraProfError.location!=null)
			System.out.println("Location: " + paraProfError.location);
		    if(paraProfError.s0!=null)
			System.out.println(paraProfError.s0);
		    if(paraProfError.s1!=null)
			System.out.println(paraProfError.s1);
		    if(paraProfError.showContactString)
			System.out.println(ParaProfError.contactString);
		}
		quit = paraProfError.quit;
	    }
	    else{
		System.out.println("An error has been detected: " + string);
	    }
	}
	else{
	    System.out.println("An error was detected at " + string);
	}
	System.out.println("####################################");
	if(quit)
	    System.exit(0);
    }
    //####################################
    //End - Error handling.
    //####################################
}
