import React from 'react';
import { createRoot } from 'react-dom/client';
import { App } from './App';
import './style.css';
import { UserPreferences } from '../features/preferences/UserPreferences';

createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <UserPreferences>
      <App />
    </UserPreferences>
  </React.StrictMode>,
);
