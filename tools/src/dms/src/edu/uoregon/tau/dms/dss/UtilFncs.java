/*
 * Name: UtilFncs.java 
 * Author: Robert Bell 
 * Description: Some useful functions for the system.
 */

package edu.uoregon.tau.dms.dss;

import java.io.File;
import java.text.DecimalFormat;
import java.util.*;

public class UtilFncs {

    
    public static class EmptyIterator implements ListIterator, Iterator {

        public int nextIndex() {
            // TODO Auto-generated method stub
            return 0;
        }

        public int previousIndex() {
            // TODO Auto-generated method stub
            return 0;
        }

        public void remove() {
            // TODO Auto-generated method stub
            
        }

        public boolean hasNext() {
            // TODO Auto-generated method stub
            return false;
        }

        public boolean hasPrevious() {
            // TODO Auto-generated method stub
            return false;
        }

        public Object next() {
            // TODO Auto-generated method stub
            return null;
        }

        public Object previous() {
            // TODO Auto-generated method stub
            return null;
        }

        public void add(Object o) {
            // TODO Auto-generated method stub
            
        }

        public void set(Object o) {
            // TODO Auto-generated method stub
            
        }
        
    }
    
    // left pad : pad string 's' up to length plen, but put the whitespace on
    // the left
    public static String lpad(String s, int plen) {
        int len = plen - s.length();
        if (len <= 0)
            return s;
        char padchars[] = new char[len];
        for (int i = 0; i < len; i++)
            padchars[i] = ' ';
        String str = new String(padchars, 0, len);
        return str.concat(s);
    }

    // pad : pad string 's' up to length plen
    public static String pad(String s, int plen) {
        int len = plen - s.length();
        if (len <= 0)
            return s;
        char padchars[] = new char[len];
        for (int i = 0; i < len; i++)
            padchars[i] = ' ';
        return s.concat(new String(padchars, 0, len));
    }

    //    public static double adjustDoublePresision(double d, int precision) {
    //        String result = null;
    //        try {
    //            String formatString = "#.#";
    //            for (int i = 0; i < (precision - 1); i++) {
    //                formatString = formatString + "#";
    //            }
    //            // 	    if(d < 0.001){
    //            // 		for(int i=0;i<4;i++){
    //            // 		    formatString = formatString+"0";
    //            // 		}
    //            // 	    }
    //
    //            formatString = formatString + "E0";
    //
    //            DecimalFormat dF = new DecimalFormat(formatString);
    //            result = dF.format(d);
    //        } catch (Exception e) {
    //            UtilFncs.systemError(e, null, "UF01");
    //        }
    //
    //        try {
    //            return Double.parseDouble(result);
    //
    //        } catch (java.lang.NumberFormatException e) {
    //            //	    System.out.println ("Uh oh! " + d);
    //            return d;
    //        }
    //    }

    // format a double for display within 'width' chars, kind of like C-printf's %G
    public static String formatDouble(double d, int width) {

        // first check if the regular toString is in exponential form
        boolean exp = false;
        String str = Double.toString(d);
        for (int i = 0; i < str.length(); i++) {
            if (str.charAt(i) == 'E') {
                exp = true;
                break;
            }
        }

        if (!exp) {
            // not exponential form
            String formatString = "";

            // create a format string of the same length, (e.g. ###.### for 123.456)
            for (int i = 0; i < str.length(); i++) {
                if (str.charAt(i) != '.')
                    formatString = formatString + "#";
                else
                    formatString = formatString + ".";
            }

            // now we reduce that format string as follows

            // first, do the minimum of 'width' or the length of the regular toString

            int min = width;
            if (formatString.length() < min)
                min = formatString.length();

            // we don't want more than 4 digits past the decimal point
            // this 4 would be the old ParaProf.defaultNumberPrecision
            if (formatString.indexOf('.') + 4 < min)
                min = formatString.indexOf('.') + 4;

            formatString = formatString.substring(0, min);

            // remove trailing dot
            if (formatString.charAt(formatString.length() - 1) == '.')
                formatString = formatString.substring(0, formatString.length() - 2);

            DecimalFormat dF = new DecimalFormat(formatString);

            str = dF.format(d);
            //System.out.println ("value: " + d + ", width: " + width + ", returning: '" + lpad(str,width) + "'");
            return lpad(str, width);

        }

        // toString used exponential form, so we ought to also

        // we want up to four significant digits
        String formatString = "0.0###";

        formatString = formatString + "E0";
        DecimalFormat dF = new DecimalFormat(formatString);

        str = dF.format(d);
        return lpad(str, width);
    }

    //This method is used in a number of windows to determine the actual output
    // string
    //displayed. Current types are:
    //0 - microseconds
    //1 - milliseconds
    //2 - seconds
    //3 - hr:min:sec
    //At present, the passed in double value is assumed to be in microseconds.
    public static String getOutputString(int type, double d, int width) {
        switch (type) {
        case 0:
            return (UtilFncs.formatDouble(d, width));
        case 1:
            return (UtilFncs.formatDouble((d / 1000), width));
        case 2:
            return (UtilFncs.formatDouble((d / 1000000), width));
        case 3:
            int hr = 0;
            int min = 0;
            hr = (int) (d / 3600000000.00);
            //Calculate the number of microseconds left after hours are
            // subtracted.
            d = d - hr * 3600000000.00;
            min = (int) (d / 60000000.00);
            //Calculate the number of microseconds left after minutes are
            // subtracted.
            d = d - min * 60000000.00;

            String hours = Integer.toString(hr);
            String mins = Integer.toString(min);

            String secs = formatDouble(d / 1000000, 7);

            // remove the whitespace
            int idx = 0;
            for (int i = 0; i < secs.length(); i++) {
                if (secs.charAt(i) != ' ') {
                    idx = i;
                    break;
                }
            }
            secs = secs.substring(idx);

            return lpad(hours + ":" + mins + ":" + secs, width);

        default:
            throw new RuntimeException("Unexpected string type: " + type);
        }
    }

    public static String getUnitsString(int type, boolean time, boolean derived) {
        if (derived) {
            if (!time)
                return "counts";
            switch (type) {
            case 0:
                return "Derived metric shown in microseconds format";
            case 1:
                return "Derived metric shown in milliseconds format";
            case 2:
                return "Derived metric shown in seconds format";
            case 3:
                return "Derived metric shown in hour:minute:seconds format";
            default:
                throw new RuntimeException("Unexpected string type: " + type);

            }
        } else {
            if (!time)
                return "counts";
            switch (type) {
            case 0:
                return "microseconds";
            case 1:
                return "milliseconds";
            case 2:
                return "seconds";
            case 3:
                return "hour:minute:seconds";
            default:
                throw new RuntimeException("Unexpected string type: " + type);
            }
        }
    }

    public static String getValueTypeString(int type) {
        switch (type) {
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
            return "number of userEvents";
        case 14:
            return "minimum number of userEvents";
        case 16:
            return "maximum number of userEvents";
        case 18:
            return "mean number of userEvents";
        case 20:
            return "Standard Deviation of User Event Value";
        default:
            throw new RuntimeException("Unexpected string type: " + type);
        }
    }

    //    public static int exists(int[] ref, int i) {
    //        if (ref == null)
    //            return -1;
    //        int test = ref.length;
    //        for (int j = 0; j < test; j++) {
    //            if (ref[j] == i)
    //                return j;
    //        }
    //        return -1;
    //    }

    public static int exists(Vector ref, int i) {
        //Assuming a vector of Integers.
        if (ref == null)
            return -1;
        Integer current = null;
        int test = ref.size();
        for (int j = 0; j < test; j++) {
            current = (Integer) ref.elementAt(j);
            if ((current.intValue()) == i)
                return j;
        }
        return -1;
    }

   

    public static DataSource initializeDataSource(File[] sourceFiles, int fileType, boolean fixGprofNames)
            throws DataSourceException {
        DataSource dataSource = null;

        List v = new ArrayList();
        File filelist[];
        switch (fileType) {
        case 0: // TAU Profiles
            FileList fl = new FileList();

            if (sourceFiles.length < 1) {
                v = fl.helperFindProfiles(System.getProperty("user.dir"));
                if (v.size() == 0)
                    throw new DataSourceException("profiles type: no profiles specified");
            } else {
                if (sourceFiles[0].isDirectory()) {
                    if (sourceFiles.length > 1) {
                        throw new DataSourceException("profiles type: you can only specify one directory");
                    }

                    v = fl.helperFindProfiles(sourceFiles[0].toString());
                    if (v.size() == 0) {
                        throw new DataSourceException("No profiles found in directory: " + sourceFiles[0]);
                    }
                } else {
                    v.add(sourceFiles);
                }

            }

            dataSource = new TauDataSource(v);
            break;
        case 1:
            if (sourceFiles.length != 1) {
                throw new DataSourceException("pprof type: you must specify exactly one file");
            }
            if (sourceFiles[0].isDirectory()) {
                throw new DataSourceException("pprof type: you must specify a file, not a directory");
            }
            v.add(sourceFiles);
            dataSource = new TauPprofDataSource(v);
            break;
        case 2:
            dataSource = new DynaprofDataSource(sourceFiles);
            break;
        case 3:

            if (sourceFiles.length != 1) {
                throw new DataSourceException("MpiP type: you must specify exactly one file");
            }
            if (sourceFiles[0].isDirectory()) {
                throw new DataSourceException("MpiP type: you must specify a file, not a directory");
            }
            dataSource = new MpiPDataSource(sourceFiles[0]);
            break;
        case 4:
            v.add(sourceFiles);
            dataSource = new HPMToolkitDataSource(v);
            break;
        case 5:
            dataSource = new GprofDataSource(sourceFiles, fixGprofNames);
            break;
        case 6:
            v.add(sourceFiles);
            dataSource = new PSRunDataSource(v);
            break;
        case 7:
            if (sourceFiles.length != 1) {
                throw new DataSourceException("Packed Profile type: you must specify exactly one file");
            }
            if (sourceFiles[0].isDirectory()) {
                throw new DataSourceException("Packed Profile type: you must specify a file, not a directory");
            }
            dataSource = new PackedProfileDataSource(sourceFiles[0]);
            break;
        case 8:
            if (sourceFiles.length != 1) {
                throw new DataSourceException("Cube type: you must specify exactly one file");
            }
            if (sourceFiles[0].isDirectory()) {
                throw new DataSourceException("Cube type: you must specify a file, not a directory");
            }
            dataSource = new CubeDataSource(sourceFiles[0]);
            break;
        default:
            break;
        }

        return dataSource;
    }

}