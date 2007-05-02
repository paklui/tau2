package edu.uoregon.tau.paraprof.tablemodel;

import java.util.ArrayList;
import java.util.List;

import javax.swing.table.AbstractTableModel;
import javax.swing.tree.DefaultTreeModel;

import edu.uoregon.tau.paraprof.ParaProfManagerWindow;
import edu.uoregon.tau.paraprof.ParaProfMetric;

public class MetricTableModel extends AbstractTableModel {

    private ParaProfMetric metric;
    private String[] columnNames = { "MetricField", "Value" };
    private List fieldNames;
    private List fieldValues;

    public MetricTableModel(ParaProfManagerWindow paraProfManager, ParaProfMetric metric, DefaultTreeModel defaultTreeModel) {
        this.metric = metric;

        fieldNames = new ArrayList();
        fieldNames.add("Name");
        fieldNames.add("Application ID");
        fieldNames.add("Experiment ID");
        fieldNames.add("Trial ID");
        fieldNames.add("Metric ID");

        fieldValues = new ArrayList();
        fieldValues.add(metric.getName());
        fieldValues.add(new Integer(metric.getApplicationID()));
        fieldValues.add(new Integer(metric.getExperimentID()));
        fieldValues.add(new Integer(metric.getTrialID()));
        fieldValues.add(new Integer(metric.getID()));
    }

    public int getColumnCount() {
        return 2;
    }

    public String getColumnName(int c) {
        return columnNames[c];
    }

    public int getRowCount() {
        return fieldNames.size();
    }

    public Object getValueAt(int r, int c) {
        if (c == 0) {
            return fieldNames.get(r);
        } else {
            return fieldValues.get(r);
        }
    }
}
