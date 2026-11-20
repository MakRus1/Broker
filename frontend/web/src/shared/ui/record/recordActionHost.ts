import { createContext, type Dispatch, type SetStateAction } from 'react';

export const RecordActionHost = createContext<{
  element: HTMLDivElement | null;
  setElement: Dispatch<SetStateAction<HTMLDivElement | null>>;
} | null>(null);
