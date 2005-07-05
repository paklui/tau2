package client;

import javax.swing.*;

import java.awt.*;
import java.util.List;
import java.util.Hashtable;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import common.*;
import edu.uoregon.tau.dms.dss.*;

public class PerformanceExplorerPane extends JScrollPane implements ActionListener {

	private static PerformanceExplorerPane thePane = null;

	private JPanel imagePanel = null;
	private Hashtable resultsHash = null;
	private RMIPerformanceResults results = null;
	private static final int imagesPerRow = 6;

	public static PerformanceExplorerPane getPane () {
		if (thePane == null) {
			JPanel imagePanel = new JPanel(new BorderLayout());
			//imagePanel.setPreferredScrollableViewportSize(new Dimension(400, 400));
			thePane = new PerformanceExplorerPane(imagePanel);
		}
		thePane.repaint();
		return thePane;
	}

	private PerformanceExplorerPane (JPanel imagePanel) {
		super(imagePanel);
		this.imagePanel = imagePanel;
		this.resultsHash = new Hashtable();
	}

	public JPanel getImagePanel () {
		return imagePanel;
	}

	public void updateImagePanel () {
		imagePanel.removeAll();
		PerfExplorerModel model = PerfExplorerModel.getModel();
		if ((model.getCurrentSelection() instanceof Metric) || 
			(model.getCurrentSelection() instanceof Trial)) {
			// check to see if we have these results already
			//results = (RMIPerformanceResults)resultsHash.get(model.toString());
			//if (results == null) {
				PerfExplorerConnection server = PerfExplorerConnection.getConnection();
				results = server.getPerformanceResults(model);
			//}
			if (results.getResultCount() == 0) {
				return;
			}
			int iStart = 0;
			List descriptions = results.getDescriptions();
			List thumbnails = results.getThumbnails();
			int imageCount = descriptions.size();
			resultsHash.put(model.toString(), results);
			JPanel imagePanel2 = null;
			// if we have 4n+1 images, then we have a dendrogram.  Put it at the top.
			if (results.getResultCount() % imagesPerRow == 1) {
				iStart = 1;
				ImageIcon icon = new ImageIcon((byte[])(thumbnails.get(0)));
				String description = (String)(descriptions.get(0));
				PerfExplorerImageButton button = new PerfExplorerImageButton(icon, 0, description);
				button.addActionListener(this);
				imagePanel.add(button, BorderLayout.CENTER);
				imagePanel2 = new JPanel(new GridLayout((results.getResultCount()-1)/imagesPerRow,imagesPerRow));
			} else {
				imagePanel2 = new JPanel(new GridLayout(results.getResultCount()/imagesPerRow,imagesPerRow));
			}

			for (int i = iStart ; i < imageCount ; i++) {
				ImageIcon icon = new ImageIcon((byte[])(thumbnails.get(i)));
				String description = (String)(descriptions.get(i));
				PerfExplorerImageButton button = new PerfExplorerImageButton(icon, i, description);
				button.addActionListener(this);
				imagePanel2.add(button);
			}
			imagePanel.add(imagePanel2, BorderLayout.SOUTH);
		}
		// this.repaint();
	}

	public void actionPerformed(ActionEvent e) {
		int index = Integer.parseInt(e.getActionCommand());
		// create a new modal dialog with the big image

		String description = (String)(results.getDescriptions().get(index));
		ImageIcon icon = new ImageIcon((byte[])(results.getImages().get(index)));
		//JOptionPane.showMessageDialog(PerfExplorerClient.getMainFrame(), null, description, JOptionPane.PLAIN_MESSAGE, icon);

        // Create and set up the window.
        JFrame frame = new JFrame(description);

        // Make the table vertically scrollable
        //JLabel label = new JLabel(icon);
        
/*        ScrollPane pane = new ScrollPane();
        pane.add(new ImageView(icon.getImage()));
*/        
        JPanel pane = new ImagePanel(icon.getImage());
        pane.setSize(500,500);
        frame.getContentPane().add(pane);
        frame.pack();
        frame.setVisible(true);
		

	}
}
