/**
 * 
 */
package edu.uoregon.tau.perfexplorer.glue;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import edu.uoregon.tau.perfdmf.Trial;
import edu.uoregon.tau.perfdmf.database.DB;
import edu.uoregon.tau.perfexplorer.server.PerfExplorerServer;

/**
 * @author khuck
 *
 */
public class TrialMeanResult extends AbstractResult {

	/**
	 * 
	 */
	private static final long serialVersionUID = -3739854995183107332L;
	private Integer originalThreads = 0;
	private boolean callPath = true;

	/**
	 * 
	 */
	public TrialMeanResult() {
		super();
	}

	/**
	 * @param input
	 */
	public TrialMeanResult(TrialMeanResult input) {
		super(input);
	}

	public TrialMeanResult(Trial trial) {
		super();
		this.trialID = trial.getID();
		this.trial = trial;
		this.name = this.trial.getName();
		buildTrialMeanResult(trial, null, null);
	}
	
	public TrialMeanResult(Trial trial, String metric, String event, boolean callPath) {
		super();
		this.trialID = trial.getID();
		this.callPath = callPath;
		this.name = this.trial.getName();
		buildTrialMeanResult(trial, metric, event);
	}
	
	private void buildTrialMeanResult(Trial trial, String metric, String event) {
		// hit the databsae, and get the data for this trial
		DB db = PerfExplorerServer.getServer().getDB();
		if (db.getSchemaVersion() > 0) {
			buildTrialResultFromTAUdb(trial, metric, event);
			return;
		}
		
		try {
			StringBuilder sql = new StringBuilder();
			sql.append("select e.name, ");
			sql.append("m.name, ");
            
            if (db.getDBType().compareTo("oracle") == 0) {
                sql.append("p.excl, ");
            } else {
                sql.append("p.exclusive, ");
            }

			sql.append("p.inclusive, ");

			if (db.getDBType().compareTo("derby") == 0) {
    			sql.append("p.num_calls, ");
            } else {
    			sql.append("p.call, ");
            }

			sql.append("p.subroutines, e.id ");
			sql.append("from interval_event e ");
			sql.append("left outer join interval_mean_summary p ");
			sql.append("on e.id = p.interval_event ");
			sql.append("left outer join metric m on m.trial = e.trial ");
			sql.append("and m.id = p.metric ");
			sql.append("where e.trial = ? ");
			if (metric != null) {
				sql.append(" and m.name = ? ");
			}
			if (event != null) {
				sql.append(" and e.name = ? ");
			}
			if (!callPath) {
            	sql.append(" and (e.group_name is null or e.group_name not like '%TAU_CALLPATH%') ");
			}
			sql.append(" order by 2,1 ");
			
			PreparedStatement statement = db.prepareStatement(sql.toString());
			//System.out.println(sql.toString() + " " + trial.getID() + " " + metric + " " + event);
			
			statement.setInt(1, trial.getID());
			int index = 1;
			if (metric != null) {
				statement.setString(index++, metric);
			}
			if (event != null) {
				statement.setString(index++, event);
			}
			//System.out.println(statement.toString());
			ResultSet results = statement.executeQuery();
			while (results.next() != false) {
				String eventName = results.getString(1);
				String metricName = results.getString(2);
				this.putExclusive(0, eventName, metricName, results.getDouble(3));
				this.putInclusive(0, eventName, metricName, results.getDouble(4));
				this.putCalls(0, eventName, results.getDouble(5));
				this.putSubroutines(0, eventName, results.getDouble(6));
				Integer eventID = results.getInt(7);
				this.eventMap.put(eventID, eventName);
			}
			results.close();
			statement.close();

		} catch (SQLException exception) {
			System.err.println(exception.getMessage());
			exception.printStackTrace();
		}
	}

	private void buildTrialResultFromTAUdb(Trial trial, String metric, String event) {
		// hit the database, and get the data for this trial
		DB db = PerfExplorerServer.getServer().getDB();
		StringBuilder sql = null;
		PreparedStatement statement = null;
		
		try {
			sql = new StringBuilder();
			if (!callPath) {
				// easy query.
        		sql.append(" select t.name, m.name, tv.exclusive_value, ");
				sql.append(" tv.inclusive_value, tcd.calls, tcd.subroutines, cp.id from timer t ");
				sql.append(" left outer join timer_callpath cp on timer.trial = " + trial.getID());
				sql.append(" and t.timer_callpath = cp.id ");
        		sql.append(" left outer join timer_call_data tcd on tcd.timer_callpath = cp.id ");
        		sql.append(" left outer join timer_value tv on tv.timer_call_data = tcd.id ");
        		sql.append(" left outer join metric m on m.trial = " + trial.getID() + " and tv.metric = m.id ");
        		sql.append(" left outer join thread h on h.trial = " + trial.getID() + " and tcd.thread = h.id ");
			} else {
         		sql.append(" with recursive cp (id, parent, timer, name) as (  ");
         		sql.append(" SELECT tc.id, tc.parent, tc.timer, timer.name  ");
         		sql.append(" FROM  timer_callpath tc inner join timer on tc.timer = timer.id ");
				sql.append(" where timer.trial = " + trial.getID() + " and tc.parent is null ");
         		sql.append(" UNION ALL ");
         		sql.append(" SELECT d.id, d.parent, d.timer, ");
				if (db.getDBType().compareTo("h2") == 0) {
					sql.append("concat (cp.name, ' => ', dt.name) ");
				} else {
					sql.append("cp.name || ' => ' || dt.name ");
				}
         		sql.append(" FROM timer_callpath AS d JOIN cp ON (d.parent = cp.id) ");
				sql.append(" join timer dt on d.timer = dt.id where dt.trial = " + trial.getID() +" ) ");
        		sql.append(" select cp.name, m.name, h.thread_index, tv.exclusive_value, ");
				sql.append(" tv.inclusive_value, tcd.calls, tcd.subroutines, cp.id from cp ");
        		sql.append(" left outer join timer_call_data tcd on tcd.timer_callpath = cp.id ");
        		sql.append(" left outer join timer_value tv on tv.timer_call_data = tcd.id ");
        		sql.append(" left outer join metric m on m.trial = " + trial.getID() + " and tv.metric = m.id ");
        		sql.append(" left outer join thread h on h.trial = " + trial.getID() + " and tcd.thread = h.id ");
            }

			sql.append("where m.trial = " + trial.getID());
			if (metric != null) {
				sql.append(" and m.name = '" + metric + "'");
			}
			if (event != null) {
				sql.append(" and cp.name = '" + event + "'");
			}
			sql.append(" and h.thread_index = -1 ");
			sql.append(" order by 3,2,1 ");
			
			statement = db.prepareStatement(sql.toString());
			
			//System.out.println(statement.toString());
			long start = System.currentTimeMillis();
			ResultSet results = statement.executeQuery();
			long elapsedTimeMillis = System.currentTimeMillis()-start;
			float elapsedTimeSec = elapsedTimeMillis/1000F;
			System.out.println("Time to query interval data: " + elapsedTimeSec + " seconds");
			while (results.next() != false) {
				String eventName = results.getString(1);
				this.putExclusive(0, eventName, results.getString(2), results.getDouble(3));
				this.putInclusive(0, eventName, results.getString(2), results.getDouble(4));
				this.putCalls(0, eventName, results.getDouble(5));
				this.putSubroutines(0, eventName, results.getDouble(6));
				Integer eventID = results.getInt(7);
				this.eventMap.put(eventID, eventName);
			}
			results.close();
			statement.close();

			// now, get the user events
			sql = new StringBuilder();
			sql.append("select c.name, ");
			sql.append("cv.sample_count, ");
			sql.append("cv.maximum_value, ");
			sql.append("cv.minimum_value, ");
			sql.append("cv.mean_value, ");
			sql.append("cv.standard_deviation ");
			sql.append("from counter c ");
			sql.append("left outer join counter_value cv ");
			sql.append("on c.id = cv.counter and c.trial = " + trial.getID());
			sql.append(" left outer join thread h ");
			sql.append("on h.id = cv.thread and h.trial = " + trial.getID());
			sql.append(" where c.trial = " + trial.getID());
			sql.append(" and h.thread_index = -1 ");
			sql.append(" order by 2,1 ");
			
			statement = db.prepareStatement(sql.toString());
			
			//System.out.println(statement.toString());
			start = System.currentTimeMillis();
			results = statement.executeQuery();
			elapsedTimeMillis = System.currentTimeMillis()-start;
			elapsedTimeSec = elapsedTimeMillis/1000F;
			System.out.println("Time to query counter data: " + elapsedTimeSec + " seconds");
			while (results.next() != false) {
				this.putUsereventNumevents(0, results.getString(1), results.getDouble(2));
				this.putUsereventMax(0, results.getString(1), results.getDouble(3));
				this.putUsereventMin(0, results.getString(1), results.getDouble(4));
				this.putUsereventMean(0, results.getString(1), results.getDouble(5));
				this.putUsereventSumsqr(0, results.getString(1), results.getDouble(6));
			}
			results.close();
			statement.close();
			
		} catch (SQLException exception) {
			System.err.println(exception.getMessage());
			exception.printStackTrace();
			if (statement != null)
				System.err.println(statement);
			else
				System.err.println(sql);
		}
	}

	public Integer getTrialID() {
		return trialID;
	}

	public void setTrialID(Integer trialID) {
		this.trialID = trialID;
	}

	/**
	 * @return the originalThreads
	 */
	public Integer getOriginalThreads() {
		return originalThreads;
	}

	/**
	 * @param originalThreads the originalThreads to set
	 */
	public void setOriginalThreads(Integer originalThreads) {
		this.originalThreads = originalThreads;
	}

}
