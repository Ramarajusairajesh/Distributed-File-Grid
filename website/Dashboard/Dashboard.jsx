import React, { useState, useEffect } from 'react';
import './Dashboard.css';

// Main Dashboard component
const Dashboard = () => {
  // State to hold the system data
  const [systems, setSystems] = useState([]);
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [selectedSystemDetails, setSelectedSystemDetails] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  // Fetch system data from API
  useEffect(() => {
    const fetchSystemData = async () => {
      try {
        setLoading(true);
        // In a real application, this would fetch from your API
        const response = await fetch('/api/systems');
        if (!response.ok) {
          throw new Error('Failed to fetch system data');
        }
        const data = await response.json();
        setSystems(data);
      } catch (err) {
        console.error('Error fetching system data:', err);
        setError(err.message);
        // Fallback to mock data for demonstration
        setSystems(getMockSystemData());
      } finally {
        setLoading(false);
      }
    };

    fetchSystemData();
    
    // Set up polling for real-time updates
    const interval = setInterval(fetchSystemData, 30000); // Update every 30 seconds
    
    return () => clearInterval(interval);
  }, []);

  // Mock data for demonstration
  const getMockSystemData = () => [
    {
      id: 'head-server-1',
      name: 'Head Server 1',
      status: 'healthy',
      type: 'head_server',
      details: {
        cpu_usage: '25%',
        memory_usage: '45%',
        disk_usage: '30%',
        active_connections: '150',
        last_check: new Date().toISOString(),
        logs: 'All systems operational. Processing 150 active connections.'
      }
    },
    {
      id: 'cluster-server-1',
      name: 'Cluster Server 1',
      status: 'healthy',
      type: 'chunk_server',
      details: {
        cpu_usage: '20%',
        memory_usage: '60%',
        disk_usage: '45%',
        stored_chunks: '1,250',
        available_storage: '2.5 TB',
        last_check: new Date().toISOString(),
        logs: 'Healthy. Storing 1,250 chunks with 2.5 TB available storage.'
      }
    },
    {
      id: 'cluster-server-2',
      name: 'Cluster Server 2',
      status: 'warning',
      type: 'chunk_server',
      details: {
        cpu_usage: '75%',
        memory_usage: '85%',
        disk_usage: '80%',
        stored_chunks: '1,180',
        available_storage: '500 GB',
        last_check: new Date().toISOString(),
        logs: 'High resource usage detected. Consider adding more storage.'
      }
    },
    {
      id: 'cluster-server-3',
      name: 'Cluster Server 3',
      status: 'healthy',
      type: 'chunk_server',
      details: {
        cpu_usage: '15%',
        memory_usage: '40%',
        disk_usage: '35%',
        stored_chunks: '980',
        available_storage: '3.2 TB',
        last_check: new Date().toISOString(),
        logs: 'Optimal performance. Ready for additional load.'
      }
    },
    {
      id: 'health-checker',
      name: 'Health Checker',
      status: 'healthy',
      type: 'health_checker',
      details: {
        cpu_usage: '5%',
        memory_usage: '20%',
        is_leader: true,
        monitored_servers: '4',
        last_check: new Date().toISOString(),
        logs: 'Leader node. Monitoring 4 servers. All systems healthy.'
      }
    },
    {
      id: 'redis',
      name: 'Redis Cache',
      status: 'healthy',
      type: 'cache',
      details: {
        cpu_usage: '10%',
        memory_usage: '30%',
        connected_clients: '5',
        keys_stored: '15,420',
        last_check: new Date().toISOString(),
        logs: 'Cache performing well. 15,420 keys stored with 5 connected clients.'
      }
    }
  ];

  // Function to handle clicking on a system card
  const handleCardClick = (system) => {
    setSelectedSystemDetails(system.details);
    setIsModalOpen(true);
  };

  // Function to close the detail modal
  const closeModal = () => {
    setIsModalOpen(false);
    setSelectedSystemDetails(null);
  };

  // Loading state
  if (loading) {
    return (
      <div className="loading-container">
        <div className="text-center">
          <div className="loading-spinner"></div>
          <p className="mt-4 text-xl">Loading system status...</p>
        </div>
      </div>
    );
  }

  // Error state
  if (error) {
    return (
      <div className="error-container">
        <div className="text-center">
          <div className="error-icon">⚠️</div>
          <h2 className="error-title">Connection Error</h2>
          <p className="error-message">{error}</p>
          <p className="error-fallback">Showing mock data for demonstration</p>
        </div>
      </div>
    );
  }

  return (
    <div className="dashboard-container">
      <header className="dashboard-header">
        <h1 className="dashboard-title">
          Distributed File Grid Dashboard
        </h1>
        <p className="dashboard-subtitle">
          Real-time monitoring of your distributed storage system
        </p>
        <div className="dashboard-timestamp">
          Last updated: {new Date().toLocaleString()}
        </div>
      </header>

      <main>
        <div className="dashboard-grid">
          {systems.map((system) => (
            <div
              key={system.id}
              className="system-card"
              onClick={() => handleCardClick(system)}
            >
              <div className="text-center">
                <h2 className="system-name">
                  {system.name}
                </h2>
                <div className="system-type">
                  {system.type.replace('_', ' ')}
                </div>
              </div>
              
              <div className="status-indicator">
                <div
                  className={`status-dot ${system.status}`}
                ></div>
                <span className={`status-text ${system.status}`}>
                  {system.status}
                </span>
              </div>
              
              <p className="click-hint">
                Click for details
              </p>
            </div>
          ))}
        </div>
      </main>

      {/* Detail Modal */}
      {isModalOpen && selectedSystemDetails && (
        <div className="modal-overlay">
          <div className="modal-content">
            <button
              onClick={closeModal}
              className="modal-close"
              aria-label="Close"
            >
              &times;
            </button>
            <h3 className="modal-title">
              System Details
            </h3>
            <div className="modal-body">
              <pre className="modal-pre">
                {JSON.stringify(selectedSystemDetails, null, 2)}
              </pre>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default Dashboard; 